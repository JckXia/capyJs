#include "builtins/fs.h"
#include "fs_context.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "uv.h"
#include "file_system.h"
#include <iostream>
#include "runtime_context.h"

JSClassID fs_class_id;

/**
  The API we intend to provide:
    -> fs.open(filePath, (err, fd) => {});
    -> fs.read(fd, buffLen, (err, data) => {});
    -> fs.close(fd) 
  
  The pitfall to prevent: The callback in open() is NOT 1:1 to file path.

  You can have stuff like
  fs.open("/tst.data", (err,fd) => { doReadOperationWithData(Fd)})
  fs.open("/tst.data", (err,fd) => { doWriteOperationWithData(fd)})

  It makes 0 sense to cache these. The problem we were seeing in production resulted
  in us pinning fd/close in a tight loop. These opens are done against the same file path, resulting 
  in us constantly overwriting the "fd" field of FieldEntry

*/
static void fs_finalizer(JSRuntime *rt, JSValue val) {
  FileSystem *fs = (FileSystem *)JS_GetOpaque(val, fs_class_id);
  if (fs) delete fs;
}

static JSClassDef fs_class_def = {.class_name = "FileSystem",
                                  .finalizer = fs_finalizer};

static JSValue fs_constructor(JSContext *ctx, JSValueConst new_target,
                              int argc, JSValueConst *argv) {
  JSValue fs_obj = JS_NewObjectClass(ctx, fs_class_id);
  JS_SetOpaque(fs_obj, new FileSystem());
  return fs_obj;
}

static void open_file_async_cb(uv_fs_t *req) {
  FileOpState *file_state = (FileOpState*)req->data;
  JSContext *ctx = file_state->ctx;

  JSValue err, fd_val;
  int fd;
  if (req->result < 0) {
    err    = JS_NewInt32(ctx, (int)req->result);
    fd_val = JS_UNDEFINED;
  } else {
    fd =  (int)req->result;
    err = JS_UNDEFINED;
    fd_val = JS_NewInt64(ctx, fd);
    file_state->fd = fd;
    file_state->owner->fd_state[fd] = file_state;
  }

  JSValue args[] = {err, fd_val};
  JSValue ret = JS_Call(ctx, file_state->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, err);
  JS_FreeValue(ctx, fd_val);
  JS_FreeValue(ctx, file_state->callback);
  file_state->callback = JS_UNDEFINED;

  uv_fs_req_cleanup(req);
  free(req);
}

static void read_file_async_cb(uv_fs_t *req) {
  FileOpState* fstate = (FileOpState*)req->data;
  JSContext *ctx = fstate->ctx;

  JSValue err, data;
  if (req->result < 0) {
    err  = JS_NewInt32(ctx, (int)req->result);
    data = JS_UNDEFINED;
  } else {
    err  = JS_UNDEFINED;
    data = JS_NewStringLen(ctx, fstate->buf, (size_t)req->result);
  }

  JSValue args[] = {err, data};
  JSValue ret = JS_Call(ctx, fstate->read_callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, err);
  JS_FreeValue(ctx, data);
  JS_FreeValue(ctx, fstate->read_callback);
  fstate->read_callback = JS_UNDEFINED;

  uv_fs_req_cleanup(req);
  free(req);
}

static void close_file_async_cb(uv_fs_t *req) {
  FileOpState *entry = (FileOpState *)req->data;
  entry->owner->fd_state.erase(entry->fd);
  entry->fd = -1;
  uv_fs_req_cleanup(req);
  free(req);
  delete entry;
}

static JSValue open_file_async(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
  FileSystem *fs = (FileSystem *)JS_GetOpaque2(ctx, this_val, fs_class_id);
  if (!fs) return JS_EXCEPTION;

  const char *path = JS_ToCString(ctx, argv[0]);
  std::string key(path);
  JS_FreeCString(ctx, path);                                


  FileOpState * unlinked_fs = new FileOpState();
  unlinked_fs->callback = JS_DupValue(ctx, argv[1]);
  unlinked_fs->ctx = ctx;
  unlinked_fs->owner = fs;

  uv_fs_t *req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  req->data = unlinked_fs;

  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  uv_fs_open(env->loop, req, key.c_str(), O_RDONLY, 0, open_file_async_cb);
  return JS_UNDEFINED;
}

static JSValue read_file_async(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
  FileSystem *fs = (FileSystem *)JS_GetOpaque2(ctx, this_val, fs_class_id);
  if (!fs) return JS_EXCEPTION;

  int64_t fd;
  JS_ToInt64(ctx, &fd, argv[0]);

  int buf_len;
  JS_ToInt32(ctx, &buf_len, argv[1]);

  //FileOpState * fstate = fs->fd_state[fd]
  auto it = fs->fd_state.find((int)fd);

  if (it == fs->fd_state.end()){
    // TODO: Move couts to an LOGGER class
    std::cerr<<"[ERR] The fd in question "<<fd<< " is not known to the file system" << std::endl; 
    return JS_UNDEFINED;
  }

  FileOpState* entry = it->second;

  if (entry->buf == nullptr || entry->buf_len != (size_t)buf_len) {
    delete[] entry->buf;
    entry->buf = new char[buf_len];
    entry->buf_len = (size_t)buf_len;
  }

  entry->read_callback = JS_DupValue(ctx, argv[2]);

  uv_fs_t *req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  req->data = entry;

  uv_buf_t uvbuf = uv_buf_init(entry->buf, buf_len);
  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  uv_fs_read(env->loop, req, (uv_file)fd, &uvbuf, 1, 0, read_file_async_cb);
  return JS_UNDEFINED;
}

static JSValue close_file_async(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
  FileSystem *fs = (FileSystem *)JS_GetOpaque2(ctx, this_val, fs_class_id);
  if (!fs) return JS_EXCEPTION;

  int64_t fd;
  JS_ToInt64(ctx, &fd, argv[0]);

  auto it = fs->fd_state.find((int)fd);
  if (it == fs->fd_state.end()) return JS_UNDEFINED;

  FileOpState *entry = it->second;

  uv_fs_t *req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  req->data = entry;

  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  uv_fs_close(env->loop, req, (uv_file)fd, close_file_async_cb);
  return JS_UNDEFINED;
}

void setup_fs_class(JSContext *ctx) {
  JSRuntime *rt = JS_GetRuntime(ctx);
  JS_NewClassID(&fs_class_id);
  JS_NewClass(rt, fs_class_id, &fs_class_def);
  JSValue fs_proto = JS_NewObject(ctx);
  JS_SetClassProto(ctx, fs_class_id, fs_proto);
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ctor = JS_NewCFunction2(ctx, fs_constructor, "FileSystem", 0,
                                  JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, fs_proto, "open",
                    JS_NewCFunction(ctx, open_file_async, "open", 2));
  JS_SetPropertyStr(ctx, fs_proto, "read",
                    JS_NewCFunction(ctx, read_file_async, "read", 3));
  JS_SetPropertyStr(ctx, fs_proto, "close",
                    JS_NewCFunction(ctx, close_file_async, "close", 1));
  JS_SetPropertyStr(ctx, global, "FileSystem", ctor);
  JS_FreeValue(ctx, global);
}

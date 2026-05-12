#include "builtins/fs.h"
#include "fs_context.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "uv.h"
#include "file_system.h"
#include "runtime_context.h"

JSClassID fs_class_id;

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
  FileEntry *entry = (FileEntry *)req->data;
  JSContext *ctx = entry->ctx;

  JSValue err, fd_val;
  if (req->result < 0) {
    err    = JS_NewInt32(ctx, (int)req->result);
    fd_val = JS_UNDEFINED;
  } else {
    entry->fd = (int)req->result;
    entry->owner->byFd[entry->fd] = entry;
    err    = JS_UNDEFINED;
    fd_val = JS_NewInt64(ctx, req->result);
  }

  JSValue args[] = {err, fd_val};
  JSValue ret = JS_Call(ctx, entry->pending_cb, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, err);
  JS_FreeValue(ctx, fd_val);
  JS_FreeValue(ctx, entry->pending_cb);
  entry->pending_cb = JS_UNDEFINED;

  uv_fs_req_cleanup(req);
  free(req);
}

static void read_file_async_cb(uv_fs_t *req) {
  FileEntry *entry = (FileEntry *)req->data;
  JSContext *ctx = entry->ctx;

  JSValue err, data;
  if (req->result < 0) {
    err  = JS_NewInt32(ctx, (int)req->result);
    data = JS_UNDEFINED;
  } else {
    err  = JS_UNDEFINED;
    data = JS_NewStringLen(ctx, entry->buf, (size_t)req->result);
  }

  JSValue args[] = {err, data};
  JSValue ret = JS_Call(ctx, entry->read_cb, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, err);
  JS_FreeValue(ctx, data);
  JS_FreeValue(ctx, entry->read_cb);
  entry->read_cb = JS_UNDEFINED;

  uv_fs_req_cleanup(req);
  free(req);
}

static void close_file_async_cb(uv_fs_t *req) {
  FileEntry *entry = (FileEntry *)req->data;
  entry->owner->byFd.erase(entry->fd);
  entry->fd = -1;
  uv_fs_req_cleanup(req);
  free(req);
}

static JSValue open_file_async(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
  FileSystem *fs = (FileSystem *)JS_GetOpaque2(ctx, this_val, fs_class_id);
  if (!fs) return JS_EXCEPTION;

  const char *path = JS_ToCString(ctx, argv[0]);
  std::string key(path);
  JS_FreeCString(ctx, path);

  auto it = fs->byPath.find(key);
  FileEntry *entry;
  if (it == fs->byPath.end()) {
    entry = new FileEntry();
    entry->ctx = ctx;
    entry->owner = fs;
    fs->byPath[key] = entry;
  } else {
    entry = it->second;
  }

  entry->pending_cb = JS_DupValue(ctx, argv[1]);

  uv_fs_t *req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  req->data = entry;

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

  auto it = fs->byFd.find((int)fd);
  if (it == fs->byFd.end()) return JS_UNDEFINED;

  FileEntry *entry = it->second;

  if (entry->buf == nullptr || entry->buf_len != (size_t)buf_len) {
    delete[] entry->buf;
    entry->buf = new char[buf_len];
    entry->buf_len = (size_t)buf_len;
  }

  entry->read_cb = JS_DupValue(ctx, argv[2]);

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

  auto it = fs->byFd.find((int)fd);
  if (it == fs->byFd.end()) return JS_UNDEFINED;

  FileEntry *entry = it->second;

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

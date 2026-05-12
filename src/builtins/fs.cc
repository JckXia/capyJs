#include "builtins/fs.h"
#include "fs_context.h" // Oh boy this is bad news
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "uv.h"
#include "file_system.h"
#include "runtime_context.h"
static JSClassDef fs_class_def = {.class_name = "FileSystem"};
JSClassID fs_class_id;

static JSValue fs_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                              JSValueConst *argv) {
  JSValue fs_obj = JS_NewObjectClass(ctx, fs_class_id);
  JSRuntime *rt = JS_GetRuntime(ctx);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);
 
  FileSystem *fs = (FileSystem *)env->allocator->alloc(sizeof(FileSystem));

  JS_SetOpaque(fs_obj, fs);
  return fs_obj;
}

struct FsCb {
  JSContext *ctx;
  JSValue js_cb;
  char *buf;     // null for open operations
  uv_fs_t *req; // owned — freed in the callback
};

static void open_file_async_cb(uv_fs_t *req) {
  FsCb *fs_with_ctx = (FsCb *)req->data;
  JSContext *ctx = fs_with_ctx->ctx;

  JSValue args[] = {JS_UNDEFINED, JS_NewInt64(ctx, req->result)};
  JS_Call(ctx, fs_with_ctx->js_cb, JS_UNDEFINED, 2, args);

  JS_FreeValue(ctx, fs_with_ctx->js_cb);
  uv_fs_req_cleanup(req);
  free(fs_with_ctx->req);
  delete fs_with_ctx;
}

static void read_file_async_cb(uv_fs_t *req) {
  FsCb *fs_with_ctx = (FsCb *)req->data;
  JSContext *ctx = fs_with_ctx->ctx;

  JSValue err, data;
  if (req->result < 0) {
    err  = JS_NewInt32(ctx, (int)req->result);
    data = JS_UNDEFINED;
  } else {
    err  = JS_UNDEFINED;
    data = JS_NewStringLen(ctx, fs_with_ctx->buf, (size_t)req->result);
  }
  JSValue args[] = {err, data};
  JS_Call(ctx, fs_with_ctx->js_cb, JS_UNDEFINED, 2, args);

  JS_FreeValue(ctx, fs_with_ctx->js_cb);
  uv_fs_req_cleanup(req);
  free(fs_with_ctx->req);
  delete[] fs_with_ctx->buf;
  delete fs_with_ctx;
}

static void close_file_async_cb(uv_fs_t *req) {
  uv_fs_req_cleanup(req);
  free(req);
}

static JSValue close_file_async(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  int64_t fd;
  JS_ToInt64(ctx, &fd, argv[0]);

  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  uv_fs_t *close_req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  uv_fs_close(env->loop, close_req, (uv_file)fd, close_file_async_cb);
  return JS_UNDEFINED;
}

static JSValue open_file_async(JSContext* ctx, JSValueConst this_val, int argc,  JSValueConst *argv) {
  const char * fp = JS_ToCString(ctx, argv[0]); 
  JSValue jsOpenCb = JS_DupValue(ctx, argv[1]);

  FileSystem * fs = (FileSystem*)JS_GetOpaque2(ctx, this_val, fs_class_id);
  
  uv_fs_t *open_req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  FsCb *fs_with_ctx = new FsCb();
  fs_with_ctx->ctx = ctx;
  fs_with_ctx->js_cb = jsOpenCb;
  fs_with_ctx->buf = nullptr;
  fs_with_ctx->req = open_req;
  fs->filepath = fp;
  open_req->data = fs_with_ctx;

  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  uv_fs_open(env->loop, open_req, fp, O_RDONLY, 0, open_file_async_cb);
  return JS_UNDEFINED;
}

static JSValue read_file_async(JSContext * ctx, JSValueConst this_val, int argc , JSValueConst *argv) {
  FileSystem * fs = (FileSystem*)JS_GetOpaque2(ctx, this_val, fs_class_id);

  JSRuntime *rt = JS_GetRuntime(ctx);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);
  
  int64_t fd;
  JS_ToInt64(ctx, &fd, argv[0]);

  int buff_len;
  JS_ToInt32(ctx, &buff_len, argv[1]);
  JSValue js_cb = JS_DupValue(ctx, argv[2]);

  char *buf = new char[buff_len];
  uv_buf_t uvbuf = uv_buf_init(buf, buff_len);
  uv_fs_t *read_req = (uv_fs_t *)malloc(sizeof(uv_fs_t));
  FsCb *fs_with_ctx = new FsCb();
  fs_with_ctx->ctx = ctx;
  fs_with_ctx->js_cb = js_cb;
  fs_with_ctx->buf = buf;
  fs_with_ctx->req = read_req;
  read_req->data = fs_with_ctx;

  uv_fs_read(env->loop, read_req, fd, &uvbuf, 1, 0, read_file_async_cb);
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
  JS_SetPropertyStr(ctx, fs_proto, "read",
                    JS_NewCFunction(ctx, read_file_async, "read", 1));
  JS_SetPropertyStr(ctx, fs_proto, "open",
                    JS_NewCFunction(ctx, open_file_async, "open", 1));
  JS_SetPropertyStr(ctx, fs_proto, "close",
                    JS_NewCFunction(ctx, close_file_async, "close", 1));
  JS_SetPropertyStr(ctx, global, "FileSystem", ctor);
  JS_FreeValue(ctx, global);
}
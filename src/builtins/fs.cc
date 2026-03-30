#include "builtins/fs.h"
#include "fs_context.h" // Oh boy this is bad news
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "runtime_context.h"
static JSClassDef fs_class_def = {.class_name = "FileSystem"};
JSClassID fs_class_id;

MappedFile mmap_file(const char* path) {
    MappedFile result = {nullptr, 0, -1};
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return result;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return result;
    }
    
    void* mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return result;
    }
    
    result.data = (char*)mapped;
    result.size = st.st_size;
    result.fd = fd;
    return result;
}

static JSValue fs_constructor(JSContext *ctx, JSValueConst new_target, int argc,
                              JSValueConst *argv) {
  JSValue obj = JS_NewObjectClass(ctx, fs_class_id);
  JSValue staticFiles = JS_GetPropertyStr(ctx, argv[0], "staticFiles");
  int is_array = JS_IsArray(ctx, staticFiles);

  if (!is_array) {
    // TODO error handling
    return JS_UNDEFINED;
  }

  uint32_t length;
  JSValue len_val = JS_GetPropertyStr(ctx, staticFiles, "length");
  JS_ToUint32(ctx, &length, len_val);
  JS_FreeValue(ctx, len_val);
   RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  for (uint32_t i = 0; i < length; i++) {
    JSValue elem = JS_GetPropertyUint32(ctx, staticFiles, i);

    JSValue obj;
    const char *str;
    size_t len;

    str = JS_ToCStringLen(ctx, &len, elem);
    MappedFile file = mmap_file(str);
    env->fs_ctx->static_files[str] = mmap_file(str); 
    JS_FreeValue(ctx, elem);
  }

  JS_FreeValue(ctx, staticFiles);
  return obj;
}

static JSValue read_file(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) { 
    RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
                                
    // JSValue v =
    const char *msg = JS_ToCString(ctx, argv[0]);
    MappedFile f = env->fs_ctx->static_files[msg]; 
                    
    JSValue result = JS_NewStringLen(ctx, f.data, f.size);
    return result;
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
                    JS_NewCFunction(ctx, read_file, "read", 1));

  JS_SetPropertyStr(ctx, global, "FileSystem", ctor);
  JS_FreeValue(ctx, global);
}
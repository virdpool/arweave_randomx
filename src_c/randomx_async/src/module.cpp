#include <node_api.h>

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type.h"
#include "macro.h"
#include "../../librandomx/src/randomx.h"

typedef void(DatasetDeallocFunc)(randomx_dataset*);

struct randomx_dataset {
  // uint8_t* memory = nullptr;
  // randomx::DatasetDeallocFunc* dealloc;
  uint8_t* memory;
  DatasetDeallocFunc* dealloc;
};


size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}


// #define RANDOMX_DATASET_BASE_SIZE 2147483648
// #define RANDOMX_DATASET_EXTRA_SIZE 33554368
// static size_t randomx_dataset_size = RANDOMX_DATASET_BASE_SIZE + RANDOMX_DATASET_EXTRA_SIZE;
static size_t randomx_dataset_size = 0;

struct Randomx_context {
  randomx_dataset*  dataset_ptr;
  randomx_cache*    cache_ptr;
  randomx_vm*       vm_ptr;
  u8*               data_src;
  size_t            data_src_len;
  i32               flags;
  volatile bool     busy; // проставляется sync частью, снимается complete
  u32               id;
  bool              is_dataset_copy;
  // thread affinity
  
};

static u32 ctx_map_uid = 0;
static std::map<u32, struct Randomx_context*> ctx_map;

#if defined(_MSC_VER)
  #include <windows.h>
  #define USE_STD_THREAD_MUTEX
#endif

#if defined(USE_STD_THREAD_MUTEX)
#include <mutex>
#include <thread>
#define THREAD_TYPE std::thread::id
#define THREAD_TYPE_AUX std::thread
std::mutex ctx_map_mutex;
void ctx_map_lock() {
  ctx_map_mutex.lock();
}
void ctx_map_unlock() {
  ctx_map_mutex.unlock();
}
std::map<std::thread::id, u32> thread_ctx_map;

#else
#include <pthread.h>
#define THREAD_TYPE pthread_t
#define THREAD_TYPE_AUX pthread_t
pthread_mutex_t ctx_map_mutex;
void ctx_map_lock() {
  pthread_mutex_lock(&ctx_map_mutex);
}
void ctx_map_unlock() {
  pthread_mutex_unlock(&ctx_map_mutex);
}
std::map<THREAD_TYPE, u32> thread_ctx_map;

#endif

static u32 thread_ctx_map_uid = 0;
static u32 thread_uid = 0;




void randomx_context_free(struct Randomx_context* ctx) {
  if (ctx->vm_ptr)
    randomx_destroy_vm(ctx->vm_ptr);
  
  if (ctx->dataset_ptr && !ctx->is_dataset_copy)
    randomx_release_dataset(ctx->dataset_ptr);
  
  if (ctx->cache_ptr)
    randomx_release_cache(ctx->cache_ptr);
  
  free(ctx->data_src);
  
  free(ctx);
}

void napi_helper_error_cb(napi_env env, const char* error_str, napi_value callback) {
  napi_status status;
  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value global (napi_get_global)");
    return;
  }
  
  napi_value call_argv[1];
  
  // napi_value error_code;
  // status = napi_create_int32(env, 1, &error_code);
  // if (status != napi_ok) {
    // printf("status = %d\n", status);
    // napi_throw_error(env, NULL, "!napi_create_int32");
    // return;
  // }
  
  napi_value error;
  status = napi_create_string_utf8(env, error_str, strlen(error_str), &error);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "!napi_create_string_utf8");
    return;
  }
  
  status = napi_create_error(env,
    NULL,
    error,
    &call_argv[0]);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    printf("error  = %s\n", error_str);
    napi_throw_error(env, NULL, "!napi_create_error");
    return;
  }
  
  napi_value result;
  status = napi_call_function(env, global, callback, 1, call_argv, &result);
  if (status != napi_ok) {
    // это нормальная ошибка если основной поток падает
    napi_throw_error(env, NULL, "!napi_call_function");
    return;
  }
  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//    init
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ctx_init.cpp"
#include "ctx_free_sync.cpp"
#include "ctx_copy_dataset_keep.cpp"

napi_value ctx_copy_dataset_keep_sync(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 ctx_id;
  status = napi_get_value_int32(env, argv[0], &ctx_id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of ctx_id");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ctx_map_lock();
  std::map<u32, Randomx_context*>::iterator it = ctx_map.find(ctx_id);
  auto end = ctx_map.end();
  if (it == end) {
    ctx_map_unlock();
    napi_throw_error(env, NULL, "bad ctx_id");
    return ret_dummy;
  }
  
  struct Randomx_context *ctx = it->second;
  ctx_map_unlock();
  if (ctx->busy) {
    napi_throw_error(env, NULL, "ctx->busy");
    return ret_dummy;
  }
  struct Randomx_context *new_ctx = (struct Randomx_context*)malloc(sizeof(struct Randomx_context));
  new_ctx->id   = ctx_map_uid++;
  new_ctx->busy = true;
  new_ctx->dataset_ptr = NULL;
  new_ctx->cache_ptr   = NULL;
  new_ctx->vm_ptr      = NULL;
  new_ctx->flags       = ctx->flags;
  new_ctx->data_src    = ctx->data_src;
  new_ctx->data_src_len= ctx->data_src_len;
  
  new_ctx->cache_ptr = randomx_alloc_cache((randomx_flags)ctx->flags);
  if (!new_ctx->cache_ptr) {
    randomx_context_free(new_ctx);
    napi_throw_error(env, NULL, "!cache_ptr randomx_alloc_cache fail");
    return ret_dummy;
  }
  new_ctx->dataset_ptr = ctx->dataset_ptr;
  
  randomx_init_cache(new_ctx->cache_ptr, new_ctx->data_src, new_ctx->data_src_len);
  
  
  new_ctx->vm_ptr = randomx_create_vm((randomx_flags)new_ctx->flags, new_ctx->cache_ptr, new_ctx->dataset_ptr);
  if (!new_ctx->vm_ptr) {
    randomx_context_free(new_ctx);
    napi_throw_error(env, NULL, "!vm_ptr randomx_create_vm fail");
    return ret_dummy;
  }
  
  ctx_map_lock();
  ctx_map.insert(std::pair<u32, Randomx_context*>(new_ctx->id, new_ctx));
  ctx_map_unlock();
  
  new_ctx->busy = false;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value ret_ctx;
  status = napi_create_int32(env, new_ctx->id, &ret_ctx);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_ctx");
    return ret_dummy;
  }
  
  
  return ret_ctx;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//    
//    randomx macro
//    
////////////////////////////////////////////////////////////////////////////////////////////////////
// одинаковый struct, одинаковый complete, но разные execute

struct Randomx_work_data {
  struct Randomx_context* ctx;
  u8* data_src;
  size_t data_src_len;
  u8* data_dst;
  size_t data_dst_len;
  
  const char* error;
  napi_ref data_dst_reference;
  napi_ref callback_reference;
};
void randomx_work_data_free(struct Randomx_work_data* worker_ctx) {
  free(worker_ctx->data_src);
  free(worker_ctx->data_dst);
  free(worker_ctx);
}

void complete_ctx_randomx_any(napi_env env, napi_status execute_status, void* _data) {
  napi_status status;
  struct Randomx_work_data* worker_ctx = (struct Randomx_work_data*)_data;
  struct Randomx_context* ctx = worker_ctx->ctx;
  if (ctx) {
    ctx->busy = false;
  }
  
  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  // //    prepare for callback (common parts)
  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value callback;
  status = napi_get_reference_value(env, worker_ctx->callback_reference, &callback);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to get referenced callback (napi_get_reference_value)");
    randomx_work_data_free(worker_ctx);
    return;
  }
  status = napi_delete_reference(env, worker_ctx->callback_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to delete reference callback_reference");
    randomx_work_data_free(worker_ctx);
    return;
  }
  
  napi_value data_dst_val;
  status = napi_get_reference_value(env, worker_ctx->data_dst_reference, &data_dst_val);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to get referenced callback (napi_get_reference_value)");
    randomx_work_data_free(worker_ctx);
    return;
  }
  status = napi_delete_reference(env, worker_ctx->data_dst_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to delete reference data_dst_reference");
    randomx_work_data_free(worker_ctx);
    return;
  }
  u8 *data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, data_dst_val, (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");
    randomx_work_data_free(worker_ctx);
    return;
  }
  
  
  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value global (napi_get_global)");
    randomx_work_data_free(worker_ctx);
    return;
  }
  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  if (execute_status != napi_ok) {
    // чтобы не дублировать код
    if (!worker_ctx->error) {
      worker_ctx->error = "execute_status != napi_ok";
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  if (worker_ctx->error) {
    napi_helper_error_cb(env, worker_ctx->error, callback);
    randomx_work_data_free(worker_ctx);
    return;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //    callback OK
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value result;
  // napi_value call_argv[0]; // WINDOWS can't compile this
  napi_value call_argv[1];
  
  // != 32 для bulk
  memcpy(data_dst, worker_ctx->data_dst, data_dst_len);
  
  status = napi_call_function(env, global, callback, 0, call_argv, &result);
  if (status != napi_ok) {
    fprintf(stderr, "status = %d\n", status);
    napi_throw_error(env, NULL, "napi_call_function FAIL");
    randomx_work_data_free(worker_ctx);
    return;
  }
  randomx_work_data_free(worker_ctx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//    randomx calc
////////////////////////////////////////////////////////////////////////////////////////////////////
// ctx_randomx_first
// ctx_randomx_next
// ctx_randomx_last
void execute_ctx_randomx_first(napi_env env, void* _data) {
  struct Randomx_work_data* data = (struct Randomx_work_data*)_data;
  randomx_calculate_hash_first(data->ctx->vm_ptr, data->data_src, data->data_src_len);
}
void execute_ctx_randomx_next(napi_env env, void* _data) {
  struct Randomx_work_data* data = (struct Randomx_work_data*)_data;
  randomx_calculate_hash_next(data->ctx->vm_ptr, data->data_src, data->data_src_len, data->data_dst);
}
void execute_ctx_randomx_last(napi_env env, void* _data) {
  struct Randomx_work_data* data = (struct Randomx_work_data*)_data;
  randomx_calculate_hash_last(data->ctx->vm_ptr, data->data_dst);
}

#include "ctx_randomx.h"
CTX_RANDOMX_TPL(ctx_randomx_first,  execute_ctx_randomx_first)
CTX_RANDOMX_TPL(ctx_randomx_next,   execute_ctx_randomx_next)
CTX_RANDOMX_TPL(ctx_randomx_last,   execute_ctx_randomx_last)

////////////////////////////////////////////////////////////////////////////////////////////////////
//    randomx bulk calc
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ctx_randomx_bulk.h"
#include "multi_ctx_randomx_bulk.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;
  
  randomx_dataset_size = randomx_dataset_item_count() * RANDOMX_DATASET_ITEM_SIZE;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  FN_EXPORT(ctx_init)
  FN_EXPORT(ctx_free_sync)
  FN_EXPORT(ctx_randomx_first)
  FN_EXPORT(ctx_randomx_next)
  FN_EXPORT(ctx_randomx_last)
  FN_EXPORT(ctx_randomx_bulk)
  FN_EXPORT(multi_ctx_randomx_bulk)
  FN_EXPORT(ctx_copy_dataset_keep)
  FN_EXPORT(ctx_copy_dataset_keep_sync)
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)

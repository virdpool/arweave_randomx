struct Init_worker_thread {
  THREAD_TYPE_AUX thread_id;
  struct Randomx_context* ctx;
  unsigned long dataset_init_start_item;
  unsigned long dataset_init_item_count;
};

static void *init_dataset_thread(void *objPtr) {
  struct Init_worker_thread *worker_ptr = (struct Init_worker_thread*) objPtr;
  struct Randomx_context* ctx = worker_ptr->ctx;
  randomx_init_dataset(
    ctx->dataset_ptr,
    ctx->cache_ptr,
    worker_ptr->dataset_init_start_item,
    worker_ptr->dataset_init_item_count
  );
  return NULL;
}

struct Init_work_data {
  struct Randomx_context* ctx;
  // u8 *data_src;
  // size_t data_src_len;
  // i32 flags;
  i32 thread_count;
  const char* error;
  napi_ref callback_reference;
};

void init_work_data_free(struct Init_work_data* worker_ctx) {
  // free(worker_ctx->data_src);
  free(worker_ctx);
}

void init_execute(napi_env env, void* _data) {
  struct Init_work_data* worker_ctx = (struct Init_work_data*)_data;
  struct Randomx_context* ctx = worker_ctx->ctx;
  u8*     data_src    = ctx->data_src;
  size_t  data_src_len= ctx->data_src_len;
  i32     flags       = ctx->flags;
  i32     thread_count= worker_ctx->thread_count;
  
  ctx->cache_ptr = NULL;
  // if (!(flags & RANDOMX_FLAG_FULL_MEM)) {
    ctx->cache_ptr = randomx_alloc_cache((randomx_flags)flags);
    if (!ctx->cache_ptr) {
      worker_ctx->error = "!cache_ptr randomx_alloc_cache fail";
      return;
    }
    randomx_init_cache(ctx->cache_ptr, data_src, data_src_len);
  // }
  ctx->dataset_ptr = randomx_alloc_dataset((randomx_flags)flags);
  if (!ctx->dataset_ptr) {
    worker_ctx->error = "!dataset_ptr randomx_alloc_dataset fail";
    return;
  }
  
  
  struct Init_worker_thread** sub_ctx_list = (struct Init_worker_thread**)malloc(sizeof(struct Init_worker_thread*)*thread_count);
  
  unsigned long start_item = 0;
  unsigned long items_per_thread;
  unsigned long items_remainder;
  
  items_per_thread = randomx_dataset_item_count() / thread_count;
  items_remainder = randomx_dataset_item_count() % thread_count;
  
  for(int i=0;i<thread_count;i++) {
    struct Init_worker_thread *sub_ctx = (struct Init_worker_thread*)malloc(sizeof(struct Init_worker_thread));
    sub_ctx->ctx = ctx;
    sub_ctx_list[i] = sub_ctx;
    sub_ctx->dataset_init_start_item = start_item;
    if (i + 1 == thread_count) {
      sub_ctx->dataset_init_item_count = items_per_thread + items_remainder;
    } else {
      sub_ctx->dataset_init_item_count = items_per_thread;
    }
    
    start_item += sub_ctx->dataset_init_item_count;
    
    #if defined(USE_STD_THREAD_MUTEX)
      sub_ctx->thread_id = std::thread(init_dataset_thread, (void*)sub_ctx);
    #else
      int err_code = pthread_create(&sub_ctx->thread_id, 0, init_dataset_thread, (void*)sub_ctx);
      if (err_code) {
        worker_ctx->error = "!pthread_create";
        return;
      }
    #endif
  }
  
  for(int i=0;i<thread_count;i++) {
    
    #if defined(USE_STD_THREAD_MUTEX)
      sub_ctx_list[i]->thread_id.join();
    #else
      pthread_join(sub_ctx_list[i]->thread_id, NULL);
    #endif
    free(sub_ctx_list[i]);
  }
  
  free(sub_ctx_list);
  
  ctx->vm_ptr = randomx_create_vm((randomx_flags)flags, ctx->cache_ptr, ctx->dataset_ptr);
  if (!ctx->vm_ptr) {
    worker_ctx->error = "!vm_ptr randomx_create_vm fail";
    return;
  }
}

void init_complete(napi_env env, napi_status execute_status, void* _data) {
  napi_status status;
  struct Init_work_data* worker_ctx = (struct Init_work_data*)_data;
  struct Randomx_context* ctx = worker_ctx->ctx;
  ctx->busy = false;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //    prepare for callback (common parts)
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value callback;
  status = napi_get_reference_value(env, worker_ctx->callback_reference, &callback);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to get referenced callback (napi_get_reference_value)");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return;
  }
  
  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value global (napi_get_global)");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  if (execute_status != napi_ok) {
    // чтобы не дублировать код
    if (!worker_ctx->error) {
      worker_ctx->error = "execute_status != napi_ok";
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  if (worker_ctx->error) {
    napi_helper_error_cb(env, worker_ctx->error, callback);
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // OK
  ctx_map_lock();
  ctx_map.insert(std::pair<u32, Randomx_context*>(ctx->id, ctx));
  ctx_map_unlock();
  // если упадем здесь то потеряем ссылку на ресурс
  
  napi_value call_argv[2];
  status = napi_get_null(env, &call_argv[0]);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value call_argv[0] !napi_get_null");
    init_work_data_free(worker_ctx);
    return;
  }
  status = napi_create_int32(env, ctx->id, &call_argv[1]);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value call_argv[1] !napi_create_int32");
    init_work_data_free(worker_ctx);
    return;
  }
  
  napi_value result;
  status = napi_call_function(env, global, callback, 2, call_argv, &result);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "!napi_call_function");
    init_work_data_free(worker_ctx);
    return;
  }
  init_work_data_free(worker_ctx);
  return;
}


napi_value ctx_init(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 4;
  napi_value argv[4];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  i32 flags;
  status = napi_get_value_int32(env, argv[1], &flags);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of flags");
    return ret_dummy;
  }
  
  i32 thread_count;
  status = napi_get_value_int32(env, argv[2], &thread_count);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of thread_count");
    return ret_dummy;
  }
  
  napi_value callback = argv[3];
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  struct Randomx_context *ctx = (struct Randomx_context*)malloc(sizeof(struct Randomx_context));
  ctx->id   = ctx_map_uid++;
  ctx->busy = true;
  ctx->dataset_ptr  = NULL;
  ctx->cache_ptr    = NULL;
  ctx->vm_ptr       = NULL;
  ctx->flags        = flags;
  ctx->data_src     = (u8*)malloc(data_src_len);
  memcpy(ctx->data_src, data_src, data_src_len);
  ctx->data_src_len = data_src_len;
  ctx->is_dataset_copy = false;
  
  struct Init_work_data* worker_ctx = (struct Init_work_data*)malloc(sizeof(struct Init_work_data));
  worker_ctx->ctx = ctx;
  worker_ctx->error = NULL;
  // worker_ctx->data_src = (u8*)malloc(data_src_len);
  // memcpy(worker_ctx->data_src, data_src, data_src_len);
  // worker_ctx->data_src_len = data_src_len;
  // worker_ctx->flags = flags;
  worker_ctx->thread_count = thread_count;
  
  status = napi_create_reference(env, callback, 1, &worker_ctx->callback_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  
  napi_value async_resource_name;
  status = napi_create_string_utf8(env, "dummy", 5, &async_resource_name);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create value async_resource_name set to \"dummy\"");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  napi_async_work work;
  status = napi_create_async_work(env,
                                   NULL,
                                   async_resource_name,
                                   init_execute,
                                   init_complete,
                                   (void*)worker_ctx,
                                   &work);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "napi_create_async_work fail");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  status = napi_queue_async_work(env, work);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "napi_queue_async_work fail");
    randomx_context_free(ctx);
    init_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  return ret_dummy;
}

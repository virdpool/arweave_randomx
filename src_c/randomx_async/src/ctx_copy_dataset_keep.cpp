struct Copy_dataset_keep_work_data {
  struct Randomx_context* ctx;
  struct Randomx_context* new_ctx;
  
  const char* error;
  napi_ref callback_reference;
};

void copy_dataset_keep_work_data_free(struct Copy_dataset_keep_work_data* worker_ctx) {
  // free(worker_ctx->data_src);
  free(worker_ctx);
}

void copy_dataset_keep_execute(napi_env env, void* _data) {
  struct Copy_dataset_keep_work_data* worker_ctx = (struct Copy_dataset_keep_work_data*)_data;
  struct Randomx_context* ctx = worker_ctx->ctx;
  struct Randomx_context* new_ctx = worker_ctx->new_ctx;
  
  
  #if defined(USE_STD_THREAD_MUTEX)
    SetThreadAffinityMask(GetCurrentThread(), 1ULL << new_ctx->id);
    Sleep(1);
  #else
    // hacky bind to NUMA node
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(new_ctx->id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      ctx_map_unlock();
      fprintf(stderr, "Error calling pthread_setaffinity_np %d\n", rc);
      worker_ctx->error = "!pthread_setaffinity_np";
      return;
    }
  #endif
  
  
  
  // UNSAFE?
  new_ctx->cache_ptr = ctx->cache_ptr;
  // new_ctx->cache_ptr = NULL;
  // // if (!(new_ctx->flags & RANDOMX_FLAG_FULL_MEM)) {
  //   new_ctx->cache_ptr = randomx_alloc_cache((randomx_flags)new_ctx->flags);
  //   if (!new_ctx->cache_ptr) {
  //     worker_ctx->error = "!cache_ptr randomx_alloc_cache fail";
  //     return;
  //   }
  //   randomx_init_cache(new_ctx->cache_ptr, new_ctx->data_src, new_ctx->data_src_len);
  // // }
  new_ctx->dataset_ptr = ctx->dataset_ptr;
  new_ctx->is_dataset_copy = true;
  
  
  
  new_ctx->vm_ptr = randomx_create_vm((randomx_flags)new_ctx->flags, new_ctx->cache_ptr, new_ctx->dataset_ptr);
  if (!new_ctx->vm_ptr) {
    worker_ctx->error = "!vm_ptr randomx_create_vm fail";
    return;
  }
  
}

void copy_dataset_keep_complete(napi_env env, napi_status execute_status, void* _data) {
  napi_status status;
  struct Copy_dataset_keep_work_data* worker_ctx = (struct Copy_dataset_keep_work_data*)_data;
  struct Randomx_context* new_ctx = worker_ctx->new_ctx;
  
  new_ctx->busy = false;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //    prepare for callback (common parts)
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value callback;
  status = napi_get_reference_value(env, worker_ctx->callback_reference, &callback);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to get referenced callback (napi_get_reference_value)");
    randomx_context_free(new_ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return;
  }
  
  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value global (napi_get_global)");
    randomx_context_free(new_ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
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
    randomx_context_free(new_ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // OK
  
  ctx_map_lock();
  ctx_map.insert(std::pair<u32, Randomx_context*>(new_ctx->id, new_ctx));
  ctx_map_unlock();
  // если упадем здесь то потеряем ссылку на ресурс
  
  napi_value call_argv[2];
  status = napi_get_null(env, &call_argv[0]);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value call_argv[0] !napi_get_null");
    copy_dataset_keep_work_data_free(worker_ctx);
    return;
  }
  status = napi_create_int32(env, new_ctx->id, &call_argv[1]);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create return value call_argv[1] !napi_create_int32");
    copy_dataset_keep_work_data_free(worker_ctx);
    return;
  }
  
  napi_value result;
  status = napi_call_function(env, global, callback, 2, call_argv, &result);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "!napi_call_function");
    copy_dataset_keep_work_data_free(worker_ctx);
    return;
  }
  copy_dataset_keep_work_data_free(worker_ctx);
  return;
}


napi_value ctx_copy_dataset_keep(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
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
  
  napi_value callback = argv[1];
  
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
  
  struct Copy_dataset_keep_work_data* worker_ctx = (struct Copy_dataset_keep_work_data*)malloc(sizeof(struct Copy_dataset_keep_work_data));
  worker_ctx->ctx = ctx;
  worker_ctx->new_ctx = new_ctx;
  worker_ctx->error = NULL;
  
  status = napi_create_reference(env, callback, 1, &worker_ctx->callback_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");
    randomx_context_free(ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  
  napi_value async_resource_name;
  status = napi_create_string_utf8(env, "dummy", 5, &async_resource_name);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create value async_resource_name set to \"dummy\"");
    randomx_context_free(ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  napi_async_work work;
  status = napi_create_async_work(env,
                                   NULL,
                                   async_resource_name,
                                   copy_dataset_keep_execute,
                                   copy_dataset_keep_complete,
                                   (void*)worker_ctx,
                                   &work);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "napi_create_async_work fail");
    randomx_context_free(ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  status = napi_queue_async_work(env, work);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "napi_queue_async_work fail");
    randomx_context_free(ctx);
    copy_dataset_keep_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  return ret_dummy;
}

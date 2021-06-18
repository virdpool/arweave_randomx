void execute_multi_ctx_randomx_bulk(napi_env env, void* _data) {
  struct Randomx_work_data* data = (struct Randomx_work_data*)_data;
  
  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  // //    ctx pick + thread affinity set
  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  // printf("REACH\n");
  ctx_map_lock();
  // printf("REACH 2\n");
  #if defined(USE_STD_THREAD_MUTEX)
    auto tid = std::this_thread::get_id();
  #else
    auto tid = pthread_self();
  #endif
  auto t_it = thread_ctx_map.find(tid);
  auto t_end = thread_ctx_map.end();
  bool found = false;
  struct Randomx_context* ctx = NULL;
  
  if (t_it != t_end) {
    u32 ctx_id = t_it->second;
    auto it = ctx_map.find(ctx_id);
    auto end = ctx_map.end();
    if (it != end) {
      found = true;
      ctx = it->second;
    } else {
      ctx_map_unlock();
      // printf("OUT 1\n");
      fprintf(stderr, "lost ctx\n");
      data->error = "lost ctx";
      return;
    }
  }
  
  if (!found) {
    while (true) {
      // printf("DEBUG thread_ctx_map_uid %d ctx_map_uid %d\n", thread_ctx_map_uid, ctx_map_uid);
      if (thread_ctx_map_uid >= ctx_map_uid) {
        ctx_map_unlock();
        // printf("OUT 2\n");
        fprintf(stderr, "thread_ctx_map_uid >= ctx_map_uid (%d >= %d)\n", thread_ctx_map_uid, ctx_map_uid);
        data->error = "thread_ctx_map_uid >= ctx_map_uid";
        return;
      }
      
      u32 ctx_id = thread_ctx_map_uid++;
      auto it = ctx_map.find(ctx_id);
      auto end = ctx_map.end();
      if (it == end) {
        printf("NOTE can't find ctx_id %d\n", ctx_id);
        continue;
      }
      
      found = true;
      ctx = it->second;
      thread_ctx_map.insert(std::pair<THREAD_TYPE, u32>(tid, ctx_id));
      
      
      #if defined(USE_STD_THREAD_MUTEX)
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << (thread_uid++));
        Sleep(1);
      #else
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(thread_uid++, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
          ctx_map_unlock();
          fprintf(stderr, "Error calling pthread_setaffinity_np %d\n", rc);
          data->error = "!pthread_setaffinity_np";
          return;
        }
      #endif
      
      ////////////////////////////////////////////////////////////////////////////////////////////////////
      
      break;
    }
  }
  
  ctx_map_unlock();
  // printf("OUT 3\n");
  
  if (ctx->busy) {
    data->error = "ctx->busy";
    return;
  }
  ctx->busy = true;
  
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //    
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  u8 *src_ptr = data->data_src;
  u8 *src_end = data->data_src + data->data_src_len;
  
  u8 *dst_ptr = data->data_dst;
  u8 *dst_end = data->data_dst + data->data_dst_len;
  
  size_t dst_chunk_length = 32;
  
  
  // int idx = 0;
  bool first = true;
  while(true) {
    if (!(src_ptr < src_end)) break;
    i32 src_chunk_length = *((i32*)src_ptr); src_ptr += 4;
    
    // printf("DEBUG idx %d %d\n", idx++, src_chunk_length);
    
    if (!(src_ptr + src_chunk_length <= src_end)) break;
    
    if (!(dst_ptr + dst_chunk_length <= dst_end)) break;
    
    if (first) {
      first = false;
      randomx_calculate_hash_first(ctx->vm_ptr, src_ptr, src_chunk_length);
      src_ptr += src_chunk_length;
      if (src_ptr == src_end) {
        randomx_calculate_hash_last(ctx->vm_ptr, dst_ptr);
        dst_ptr += dst_chunk_length;
      }
    } else {
      randomx_calculate_hash_next(ctx->vm_ptr, src_ptr, src_chunk_length, dst_ptr);
      src_ptr += src_chunk_length;
      dst_ptr += dst_chunk_length;
      if (dst_ptr >= dst_end) {
        fprintf(stderr, "dst last out of bounds\n");
      }
      if (src_ptr == src_end) {
        randomx_calculate_hash_last(ctx->vm_ptr, dst_ptr);
        dst_ptr += dst_chunk_length;
      }
    }
  }
  
  if (src_ptr != src_end) {
    fprintf(stderr, "bad exit src_ptr != src_end (%ld != %ld)\n", (size_t)src_ptr, (size_t)src_end);
  }
  if (dst_ptr != dst_end) {
    fprintf(stderr, "bad exit dst_ptr != dst_end (%ld != %ld)\n", (size_t)dst_ptr, (size_t)dst_end);
  }
  ctx->busy = false;
}

napi_value multi_ctx_randomx_bulk(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 3;
  napi_value argv[3];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  u8 *data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[1], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");
    return ret_dummy;
  }
  napi_value data_dst_val = argv[1];
  
  napi_value callback = argv[2];
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/
  if (data_dst_len % 32 != 0) {
    printf("data_dst_len %ld\n", data_dst_len);
    napi_helper_error_cb(env, "Invalid buffer was passed as argument of data_dst; length % 32 != 0", callback);
    return ret_dummy;
  }
  
  
  struct Randomx_work_data* worker_ctx = (struct Randomx_work_data*)malloc(sizeof(struct Randomx_work_data));
  worker_ctx->ctx = NULL;
  worker_ctx->error = NULL;
  
  worker_ctx->data_src = (u8*)malloc(data_src_len);
  memcpy(worker_ctx->data_src, data_src, data_src_len);
  worker_ctx->data_src_len = data_src_len;
  
  worker_ctx->data_dst = (u8*)malloc(data_dst_len);
  worker_ctx->data_dst_len = data_dst_len;
  
  status = napi_create_reference(env, callback, 1, &worker_ctx->callback_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");
    randomx_work_data_free(worker_ctx);
    return ret_dummy;
  }
  /* EXTRA */
  status = napi_create_reference(env, data_dst_val, 1, &worker_ctx->data_dst_reference);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");
    randomx_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  napi_value async_resource_name;
  status = napi_create_string_utf8(env, "dummy", 5, &async_resource_name);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "Unable to create value async_resource_name set to \"dummy\"");
    randomx_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  napi_async_work work;
  status = napi_create_async_work(env,
                                  NULL,
                                  async_resource_name,
                                  execute_multi_ctx_randomx_bulk,
                                  complete_ctx_randomx_any,
                                  (void*)worker_ctx,
                                  &work);
  if (status != napi_ok) {
    printf("status = %d\n", status);
    napi_throw_error(env, NULL, "napi_create_async_work fail");
    randomx_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  status = napi_queue_async_work(env, work);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "napi_queue_async_work fail");
    randomx_work_data_free(worker_ctx);
    return ret_dummy;
  }
  
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/
  return ret_dummy;
}



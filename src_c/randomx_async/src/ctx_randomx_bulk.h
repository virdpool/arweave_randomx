void execute_ctx_randomx_bulk(napi_env env, void* _data) {
  struct Randomx_work_data* data = (struct Randomx_work_data*)_data;
  
  u8 *src_ptr = data->data_src;
  u8 *src_end = data->data_src + data->data_src_len;
  
  u8 *dst_ptr = data->data_dst;
  u8 *dst_end = data->data_dst + data->data_dst_len;
  
  size_t dst_chunk_length = 32;
  
  
  bool first = true;
  while(true) {
    if (!(src_ptr < src_end)) break;
    i32 src_chunk_length = *((i32*)src_ptr); src_ptr += 4;
    if (!(src_ptr + src_chunk_length <= src_end)) break;
    
    if (!(dst_ptr + dst_chunk_length <= dst_end)) break;
    
    if (first) {
      first = false;
      randomx_calculate_hash_first(data->ctx->vm_ptr, src_ptr, src_chunk_length);
      src_ptr += src_chunk_length;
      if (src_ptr == src_end) {
        randomx_calculate_hash_last(data->ctx->vm_ptr, dst_ptr);
        dst_ptr += dst_chunk_length;
      }
    } else {
      randomx_calculate_hash_next(data->ctx->vm_ptr, src_ptr, src_chunk_length, dst_ptr);
      src_ptr += src_chunk_length;
      dst_ptr += dst_chunk_length;
      if (dst_ptr >= dst_end) {
        fprintf(stderr, "last dst out of bounds\n");
      }
      if (src_ptr == src_end) {
        randomx_calculate_hash_last(data->ctx->vm_ptr, dst_ptr);
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
}

napi_value ctx_randomx_bulk(napi_env env, napi_callback_info info) {
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
  
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/
  i32 ctx_id;
  status = napi_get_value_int32(env, argv[0], &ctx_id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of ctx_id");
    return ret_dummy;
  }
  
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[1], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  u8 *data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[2], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");
    return ret_dummy;
  }
  napi_value data_dst_val = argv[2];
  
  napi_value callback = argv[3];
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/
  if (data_dst_len % 32 != 0) {
    printf("data_dst_len %ld\n", data_dst_len);
    napi_helper_error_cb(env, "Invalid buffer was passed as argument of data_dst; length % 32 != 0", callback);
    return ret_dummy;
  }
  
  ctx_map_lock();
  std::map<u32, Randomx_context*>::iterator it = ctx_map.find(ctx_id);
  auto end = ctx_map.end();
  ctx_map_unlock();
  if (it == end) {
    napi_helper_error_cb(env, "bad ctx_id", callback);
    return ret_dummy;
  }
  
  struct Randomx_context *ctx = it->second;
  if (ctx->busy) {
    napi_helper_error_cb(env, "ctx->busy", callback);
    return ret_dummy;
  }
  ctx->busy = true;
  
  struct Randomx_work_data* worker_ctx = (struct Randomx_work_data*)malloc(sizeof(struct Randomx_work_data));
  worker_ctx->ctx = ctx;
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
                                  execute_ctx_randomx_bulk,
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



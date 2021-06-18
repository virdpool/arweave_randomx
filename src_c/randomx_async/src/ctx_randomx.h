#define CTX_RANDOMX_TPL(FN_NAME, EXECUTE_FN_NAME)                                                                                 \
  /*//////////////////////////////////////////////////////////////////////////////////////////////////*/                          \
  napi_value FN_NAME(napi_env env, napi_callback_info info) {                                                                     \
    napi_status status;                                                                                                           \
                                                                                                                                  \
    napi_value ret_dummy;                                                                                                         \
    status = napi_create_int32(env, 0, &ret_dummy);                                                                               \
                                                                                                                                  \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "Unable to create return value ret_dummy");                                                     \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    size_t argc = 4;                                                                                                              \
    napi_value argv[4];                                                                                                           \
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);                                                                \
                                                                                                                                  \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "Failed to parse arguments");                                                                   \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    /*//////////////////////////////////////////////////////////////////////////////////////////////////*/                        \
    i32 ctx_id;                                                                                                                   \
    status = napi_get_value_int32(env, argv[0], &ctx_id);                                                                         \
                                                                                                                                  \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "Invalid i32 was passed as argument of ctx_id");                                                \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    u8 *data_src;                                                                                                                 \
    size_t data_src_len;                                                                                                          \
    status = napi_get_buffer_info(env, argv[1], (void**)&data_src, &data_src_len);                                                \
                                                                                                                                  \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");                                           \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    u8 *data_dst;                                                                                                                 \
    size_t data_dst_len;                                                                                                          \
    status = napi_get_buffer_info(env, argv[2], (void**)&data_dst, &data_dst_len);                                                \
                                                                                                                                  \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");                                           \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
    napi_value data_dst_val = argv[2];                                                                                            \
                                                                                                                                  \
    napi_value callback = argv[3];                                                                                                \
    /*//////////////////////////////////////////////////////////////////////////////////////////////////*/                        \
    if (data_dst_len != 32) {                                                                                                     \
      printf("data_dst_len %ld\n", data_dst_len);                                                                                 \
      napi_helper_error_cb(env, "Invalid buffer was passed as argument of data_dst; length != 32", callback);                     \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    ctx_map_lock();                                                                                                               \
    std::map<u32, Randomx_context*>::iterator it = ctx_map.find(ctx_id);                                                          \
    auto end = ctx_map.end();                                                                                                     \
    ctx_map_unlock();                                                                                                             \
    if (it == end) {                                                                                                              \
      napi_helper_error_cb(env, "bad ctx_id", callback);                                                                          \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    struct Randomx_context *ctx = it->second;                                                                                     \
    if (ctx->busy) {                                                                                                              \
      napi_helper_error_cb(env, "ctx->busy", callback);                                                                           \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
    ctx->busy = true;                                                                                                             \
                                                                                                                                  \
    struct Randomx_work_data* worker_ctx = (struct Randomx_work_data*)malloc(sizeof(struct Randomx_work_data));                   \
    worker_ctx->ctx = ctx;                                                                                                        \
    worker_ctx->error = NULL;                                                                                                     \
                                                                                                                                  \
    worker_ctx->data_src = (u8*)malloc(data_src_len);                                                                             \
    memcpy(worker_ctx->data_src, data_src, data_src_len);                                                                         \
    worker_ctx->data_src_len = data_src_len;                                                                                      \
                                                                                                                                  \
    worker_ctx->data_dst = (u8*)malloc(data_dst_len);                                                                             \
                                                                                                                                  \
    status = napi_create_reference(env, callback, 1, &worker_ctx->callback_reference);                                            \
    if (status != napi_ok) {                                                                                                      \
      printf("status = %d\n", status);                                                                                            \
      napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");                              \
      randomx_work_data_free(worker_ctx);                                                                                         \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
    /* EXTRA */                                                                                                                   \
    status = napi_create_reference(env, data_dst_val, 1, &worker_ctx->data_dst_reference);                                        \
    if (status != napi_ok) {                                                                                                      \
      printf("status = %d\n", status);                                                                                            \
      napi_throw_error(env, NULL, "Unable to create reference for callback. napi_create_reference");                              \
      randomx_work_data_free(worker_ctx);                                                                                         \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    napi_value async_resource_name;                                                                                               \
    status = napi_create_string_utf8(env, "dummy", 5, &async_resource_name);                                                      \
    if (status != napi_ok) {                                                                                                      \
      printf("status = %d\n", status);                                                                                            \
      napi_throw_error(env, NULL, "Unable to create value async_resource_name set to \"dummy\"");                                 \
      randomx_work_data_free(worker_ctx);                                                                                         \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    napi_async_work work;                                                                                                         \
    status = napi_create_async_work(env,                                                                                          \
                                     NULL,                                                                                        \
                                     async_resource_name,                                                                         \
                                     EXECUTE_FN_NAME,                                                                             \
                                     complete_ctx_randomx_any,                                                                    \
                                     (void*)worker_ctx,                                                                           \
                                     &work);                                                                                      \
    if (status != napi_ok) {                                                                                                      \
      printf("status = %d\n", status);                                                                                            \
      napi_throw_error(env, NULL, "napi_create_async_work fail");                                                                 \
      randomx_work_data_free(worker_ctx);                                                                                         \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    status = napi_queue_async_work(env, work);                                                                                    \
    if (status != napi_ok) {                                                                                                      \
      napi_throw_error(env, NULL, "napi_queue_async_work fail");                                                                  \
      randomx_work_data_free(worker_ctx);                                                                                         \
      return ret_dummy;                                                                                                           \
    }                                                                                                                             \
                                                                                                                                  \
    /*//////////////////////////////////////////////////////////////////////////////////////////////////*/                        \
    return ret_dummy;                                                                                                             \
  }                                                                                                                               \



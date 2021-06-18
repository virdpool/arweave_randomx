napi_value ctx_free_sync(napi_env env, napi_callback_info info) {
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
  if (ctx->busy) {
    ctx_map_unlock();
    napi_throw_error(env, NULL, "ctx->busy");
    return ret_dummy;
  }
  
  ctx->busy = true;
  randomx_context_free(ctx);
  ctx_map.erase(it);
  ctx_map_unlock();
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  return ret_dummy;
}

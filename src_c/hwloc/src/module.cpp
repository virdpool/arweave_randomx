#include <node_api.h>

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type.h"
#include "macro.h"
#if defined(_MSC_VER)
  #include "include/hwloc.h"
#else
  #include <hwloc.h>
#endif

napi_value hwloc_dataset_count_get(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 0;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  hwloc_topology_t m_topology = NULL;
  
  hwloc_topology_init(&m_topology);
  hwloc_topology_load(m_topology);
  i32 m_nodes  = hwloc_bitmap_weight(hwloc_topology_get_complete_nodeset(m_topology));
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_dataset_count;
  status = napi_create_int32(env, m_nodes, &ret_dataset_count);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dataset_count");
    return ret_dataset_count;
  }
  
  return ret_dataset_count;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  FN_EXPORT(hwloc_dataset_count_get)
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)

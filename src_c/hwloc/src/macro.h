#pragma once

#define PPCAT_NX(A, B) A ## B
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define FN_EXPORT(NAME)                                                 \
status = napi_create_function(env, NULL, 0, NAME, NULL, &fn);           \
if (status != napi_ok) {                                                \
  napi_throw_error(env, NULL, "Unable to wrap native function");        \
}                                                                       \
                                                                        \
status = napi_set_named_property(env, exports, TOSTRING(NAME), fn);     \
if (status != napi_ok) {                                                \
  napi_throw_error(env, NULL, "Unable to populate exports");            \
}


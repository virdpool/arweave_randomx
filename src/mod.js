if (process.platform == "win32") {
  global.randomx_async = require("../src_c/randomx_async/win/build/Release/module.node")
  global.hwloc = require("../src_c/hwloc/win/build/Release/module.node")
} else {
  var arch = "native";
  var arch = "x86-64"
  global.randomx_async = require("../src_c/randomx_async/linux/build_"+arch+"/Release/module.node")
  global.hwloc = require("../src_c/hwloc/linux/build_"+arch+"/Release/module.node")
}

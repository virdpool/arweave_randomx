#!/usr/bin/env iced
fs = require "fs"
{execSync} = require "child_process"
require "fy"

arch_list = fs.readFileSync("arch_list", "utf-8")
  .split("\n")
  .filter (t)->t
  .filter (t)->!/^#/.test t

p "arch_list"
for v in arch_list
  p "  #{v}"
p ""


conf =
  private : true
  # name    : "cs_miner"
  name    : "arweave_randomx"
  version : "1.0.0" # TODO better version
  description: ""
  bin     : "index.js"
  pkg :
    # "scripts": ["*.js", "../src/*.js"]
    assets: [
      "*.node"
    ]
    targets: [
      "node12-linux-x64"
    ]
    outputPath: "dist"
  

# ###################################################################################################
#    clear
# ###################################################################################################
cmd = "rm -rf build/* || echo -e ''"
execSync cmd

# ###################################################################################################
#    iced compile
# ###################################################################################################
cmd = "mkdir -p build/src_inter"
execSync cmd
cmd = "iced -o build/src_inter -c src"
execSync cmd


# ###################################################################################################
#    
#    linux
#    
# ###################################################################################################
for arch in arch_list
  p arch
  
  # ###################################################################################################
  #    copy binary modules
  # ###################################################################################################
  cmd = "mkdir -p build/src_inter_linux_#{arch}"
  execSync cmd
  cmd = "cp src_c/randomx_async/linux/build_#{arch}/Release/module.node build/src_inter_linux_#{arch}/randomx_async_12.node"
  execSync cmd
  cmd = "cp src_c/hwloc/linux/build_#{arch}/Release/module.node build/src_inter_linux_#{arch}/hwloc_12.node"
  execSync cmd
  
  fs.writeFileSync "build/src_inter_linux_#{arch}/mod.js", """
    global.randomx_async = require("./randomx_async_12.node");
    global.hwloc = require("./hwloc_12.node");
    """#"
  
  # ###################################################################################################
  #    copy src
  # ###################################################################################################
  cmd = "cp -r build/src_inter/* build/src_inter_linux_#{arch}"
  execSync cmd
  
  fs.writeFileSync "build/src_inter_linux_#{arch}/package.json", JSON.stringify conf, null, 2

# ###################################################################################################
#    
#    win
#    
# ###################################################################################################
# ###################################################################################################
#    copy binary modules
# ###################################################################################################
cmd = "mkdir -p build/src_inter_win"
execSync cmd
cmd = "cp src_c/randomx_async/win/build/Release/module.node build/src_inter_win/randomx_async_12.node"
execSync cmd
cmd = "cp src_c/hwloc/win/build/Release/module.node build/src_inter_win/hwloc_12.node"
execSync cmd

fs.writeFileSync "build/src_inter_win/mod.js", """
  global.randomx_async = require("./randomx_async_12.node")
  global.hwloc = require("./hwloc_12.node")
  """#"

# ###################################################################################################
#    copy src
# ###################################################################################################
cmd = "cp -r build/src_inter/* build/src_inter_win"
execSync cmd

conf.pkg.targets = ["node12-win-x64"]
fs.writeFileSync "build/src_inter_win/package.json", JSON.stringify conf, null, 2

# ###################################################################################################
build_list = fs.readdirSync "build"

p "build_list"
for v in build_list
  p "  #{v}"
p ""
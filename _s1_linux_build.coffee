#!/usr/bin/env iced
fs = require "fs"
{
  exec
  execSync
} = require "child_process"
require "fy"
require "lock_mixin"
colors = require "colors"
argv = require("minimist")(process.argv.slice(2))

arch_list = fs.readFileSync("arch_list", "utf-8")
  .split("\n")
  .filter (t)->t
  .filter (t)->!/^#/.test t

p "arch_list"
for v in arch_list
  p "  #{v}"
p ""

# ###################################################################################################
if !argv.skip_librandomx
  p "librandomx"
  lock = new Lock_mixin
  lock.$limit = 4
  
  need_throw_tuple = null
  complete_count = 0
  in_progress_list = []
  progress_log = ()->
    p "done #{complete_count}/#{arch_list.length} #{in_progress_list.slice(0, 4).join ' '}"
  progress_log()
  await
    for arch in arch_list
      if arch != "native"
        cmd = "./build_march.sh #{arch} -DARCH_ID=#{arch}"
      else
        cmd = "./build_march.sh #{arch}"
      
      cb = defer()
      do (arch, cmd, cb)->
        await lock.wrap cb, defer(cb)
        in_progress_list.push arch
        progress_log()
        await exec cmd, {cwd: "src_c/librandomx"}, defer(err, stdout, stderr)
        if err
          need_throw_tuple = [err, stdout, stderr]
        complete_count++
        in_progress_list.remove arch
        progress_log()
        cb()
  
  if need_throw_tuple
    [err, stdout, stderr] = need_throw_tuple
    p colors.green "####################################################################################################"
    p colors.green "stdout"
    p err.stdout
    p colors.green "####################################################################################################"
    
    p ""
    
    p colors.red "####################################################################################################"
    p colors.red "stderr"
    p err.stderr
    p colors.red "####################################################################################################"
    
    p ""
    p err.message
    process.exit()

# ###################################################################################################
for mod_name in ["randomx_async", "hwloc"]
  p mod_name
  
  opt =
    cwd : "src_c/#{mod_name}/linux"
  
  if argv.verbose
    opt.stdio = "inherit"
  
  cmd = "rm -rf build 2>/dev/null || echo -e ''"
  execSync cmd, opt
  
  for arch in arch_list
    p arch
    fs.writeFileSync "src_c/#{mod_name}/linux/arch", arch
    cmd = "rm -rf build_#{arch} 2>/dev/null || echo -e ''"
    execSync cmd, opt
    
    cmd = "npm run install"
    execSync cmd, opt
    cmd = "mv build build_#{arch}"
    execSync cmd, opt



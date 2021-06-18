#!/usr/bin/env iced
fs = require "fs"
{execSync} = require "child_process"
require "fy"
argv = require("minimist")(process.argv.slice(2))

build_list = fs.readdirSync "build"

p "build_list"
for v in build_list
  p "  #{v}"
p ""

# ###################################################################################################
#    clear
# ###################################################################################################
cmd = "mkdir -p dist"
execSync cmd
cmd = "rm -rf dist/*"
execSync cmd

# ###################################################################################################

for build in build_list
  continue if build == "src_inter"
  if argv.build?
    continue if build != argv.build
  p build
  
  # compress=Gzip усложняет string patching
  cmd = "npx pkg --compress=Gzip build/#{build}"
  execSync cmd
  
  # подчищаем мусор
  cmd = "rm build/#{build}/*.bak 2>/dev/null || echo -e ''"
  execSync cmd

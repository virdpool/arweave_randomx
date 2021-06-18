#!/usr/bin/env iced
# UV_THREADPOOL_SIZE=$(nproc)
crypto  = require "crypto"
{exec} = require "child_process"
os = require "os"
require "fy"
require "lock_mixin"
require "./mod"
randomx = require "./randomx"
argv = require("minimist")(process.argv.slice(2))
for k,v of argv
  k = k.split("-").join("_")
  argv[k] = v

if argv.help
  p """
  usage
    UV_THREADPOOL_SIZE=$(nproc) ./gimmebat_miner
    
      --thread-count=16   (max thread usage)
      --batch-task-count  (default:60)
      --max-duration      (default:30 how long perform test (in seconds))
      
      # randomx params
      --large-pages       (use large pages. CAN CRASH)
      --full-mem          (consume more memory, better effeciency)
      --hard-aes          (CAN CRASH if not available on your CPU)
      --argon2-ssse3      (CAN CRASH if not available on your CPU)
      --jit
      --secure            (secure jit)
      
  """
  process.exit()

# ###################################################################################################
#    argv
# ###################################################################################################
max_duration = 1000*+(argv.max_duration ? "30")

flags = 0 |
flags |= randomx.FLAG_JIT           if argv.jit
flags |= randomx.FLAG_FULL_MEM      if argv.full_mem
flags |= randomx.FLAG_HARD_AES      if argv.hard_aes
flags |= randomx.FLAG_ARGON2_SSSE3  if argv.argon2_ssse3
flags |= randomx.FLAG_SECURE        if argv.secure
flags |= randomx.FLAG_LARGE_PAGES   if argv.large_pages
batch_task_count = +(argv.batch_task_count ? "60")

p "DEBUG flags", flags

thread_count = os.cpus().length
ctx_count = argv.thread_count ? thread_count
p "thread count #{ctx_count}"

# ###################################################################################################
#    misc
# ###################################################################################################
key = Buffer.from "012345678901234567890123456789012345678912"

can_monitor_and_alloc_large_pages = process.platform != "win32"

dataset_count = 0
if argv.dataset_count
  dataset_count = +argv.dataset_count 
else
  p "dataset_count autodetect"
  try
    dataset_count = hwloc.hwloc_dataset_count_get()
  catch err
    perr err
  
  if dataset_count <= 0
    p "HWLOC FAILED topology detection"
    p "FALLBACK"
    dataset_count = 1

if ctx_count % dataset_count
  p "WARNING ctx_count % dataset_count != 0 #{ctx_count} % #{dataset_count} = #{ctx_count % dataset_count}"
  p "FALLBACK"
  dataset_count = 1

dataset_ctx_count = ctx_count/dataset_count

p "dataset_count = #{dataset_count}"

if argv.large_pages
  should_left = 2 * (1024**3)
  pages_can_alloc = (os.totalmem() - should_left) // (2 * 1024*1024)
  pages_need = 400*dataset_count + 1*(ctx_count)
  
  if pages_need > pages_can_alloc
    p "not enough memory for large pages"
    p "pages_need = #{pages_need}"
    p "pages_can_alloc = #{pages_can_alloc}"
    process.exit()
  
  if can_monitor_and_alloc_large_pages
    if pages_need*2 < pages_can_alloc
      p "can allocate extended memory"
      pages_need *= 2
    
    # dumb way
    cmd = "cat /proc/meminfo | grep HugePages_Total"
    await exec cmd, defer(err, stdout); throw err if err
    total_page_count = +stdout.split(":")[1].trim()
    
    cmd = "cat /proc/meminfo | grep HugePages_Free"
    await exec cmd, defer(err, stdout); throw err if err
    free_page_count = +stdout.split(":")[1].trim()
    
    if free_page_count < pages_need + (total_page_count - free_page_count)
      # LINUX only
      cmd = "sysctl -w vm.nr_hugepages=#{pages_need + (total_page_count - free_page_count)}"
      p cmd
      await exec cmd, defer(err, stdout, stderr);
      if err
        p "failed"
        p stdout
        p stderr
        process.exit()
    else
      p "some memory is probably occupied with other large pages stuff"
  
  if can_monitor_and_alloc_large_pages
    cmd = "cat /proc/meminfo | grep HugePages_Total"
    await exec cmd, defer(err, stdout); throw err if err
    total_page_count = +stdout.split(":")[1].trim()
    
    cmd = "cat /proc/meminfo | grep HugePages_Free"
    await exec cmd, defer(err, stdout); throw err if err
    free_page_count = +stdout.split(":")[1].trim()
    
    if free_page_count < pages_need
      mem_mb = pages_need
      p "not enough free large pages"
      p "total  : #{total_page_count}"
      p "free   : #{free_page_count}"
      p "need   : #{pages_need}"
      p "you have some other apps that are using large pages"
      p "  sysctl -w vm.nr_hugepages=#{pages_need + (total_page_count - free_page_count)}"
      p "for #{ctx_count} threads I need #{mem_mb}MB RAM for large pages (will be occupied after executing this command)"
      process.exit()

# ###################################################################################################
#    init
# ###################################################################################################
ctx_list = []
p "ctx_init (VERY LONG operation)..."

for dataset_idx in [0 ... dataset_count]
  puts "dataset #{dataset_idx+1}"
  
  await randomx_async.ctx_init key, flags, ctx_count, defer(err, ctx); throw err if err
  ctx_list.push {
    ctx
    free : true
  }
  
  need_throw_err = null
  await
    for i in [1 ... dataset_ctx_count] by 1
      cb = defer()
      do (i, cb)->
        # p "#{i} start"
        await randomx_async.ctx_copy_dataset_keep ctx, defer(err, new_ctx);
        if err
          perr err.message
          need_throw_err = true
          return cb()
        # p "#{i} complete"
        ctx_list.push {
          ctx  : new_ctx
          free : true
        }
        cb()
  throw need_throw_err if need_throw_err

p "ctx_init done"
ctx_lock = new Lock_mixin
ctx_lock.$limit = ctx_count
free_src_dst_pair_list = []

# ###################################################################################################
task_size = 32+48
res_size  = 32

start_ts = Date.now()

hashrate = 0
hashrate_total = 0
work_in_progress = true
do ()->
  while work_in_progress
    p "hashrate", hashrate
    hashrate = 0
    await setTimeout defer(), 1000
  return

p "benchmark max duration #{max_duration/1000} sec"
need_throw_err = null
loop
  elp_ts = Date.now() - start_ts
  if elp_ts > max_duration
    puts "stopping..."
    break
  break if need_throw_err
  if !src_dst_buf_pair = free_src_dst_pair_list.pop()
    src_buf = Buffer.alloc (4+task_size)*batch_task_count
    dst_buf = Buffer.alloc res_size*batch_task_count
    
    crypto.randomFillSync src_buf
    
    task_offset = 0
    for task_idx in [0 ... batch_task_count]
      src_buf.writeUInt32LE task_size, task_offset
      task_offset += 4
      task_offset += task_size
    
    src_dst_buf_pair = [src_buf, dst_buf]
  
  await ctx_lock.lock defer()
  do (src_dst_buf_pair)->
    [src_buf, dst_buf] = src_dst_buf_pair
    await randomx_async.multi_ctx_randomx_bulk src_buf, dst_buf, defer(err);
    ctx_lock.unlock()
    
    hashrate += batch_task_count
    hashrate_total += batch_task_count
    if err
      need_throw_err = err
      return
    free_src_dst_pair_list.push src_dst_buf_pair

await ctx_lock.drain defer()
work_in_progress = false
throw need_throw_err if need_throw_err

elp_ts = Date.now() - start_ts

hashrate = (hashrate_total)/(elp_ts/1000)
p "avg hashrate #{hashrate.toFixed 2}"

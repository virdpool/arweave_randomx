#!/bin/bash
UV_THREADPOOL_SIZE=$(nproc) ./arweave_randomx --large-pages --jit --full-mem --hard-aes --argon2-ssse3

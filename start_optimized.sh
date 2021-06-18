#!/bin/bash
UV_THREADPOOL_SIZE=$(nproc) ./arweave_randomx --jit --full-mem --hard-aes --argon2-ssse3

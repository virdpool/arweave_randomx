# arweave_randomx

benchmarker for arweave_randomx

# binaries

  * windows 7 .. 10 https://virdpool.com/arweave_randomx_dist/arweave_randomx.exe
    * https://virdpool.com/arweave_randomx_dist/arweave_randomx/start_optimized.bat
    * https://virdpool.com/arweave_randomx_dist/arweave_randomx/start_optimized2.bat
  * linux (ubuntu 18.04) https://virdpool.com/arweave_randomx_dist/arweave_randomx
    * https://virdpool.com/arweave_randomx_dist/arweave_randomx/start_optimized.sh
    * https://virdpool.com/arweave_randomx_dist/arweave_randomx/start_optimized2.sh

## launch on Windows

    start_optimized.bat
    // OR (with large pages)
    start_optimized2.bat

## launch on Linux

    ./start_optimized.sh
    // OR (with large pages)
    ./start_optimized2.sh

# how to build

    # install deps
    apt-get install libhwloc5 libhwloc-dev
    git clone https://github.com/virdpool/arweave_randomx
    cd arweave_randomx
    # install https://github.com/nvm-sh/nvm
    # curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.38.0/install.sh | bash
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.38.0/install.sh | bash
    source ~/.bashrc
    nvm i 14
    npm i -g iced-coffee-script
    npm ci
    
    ./_s1_linux_build.coffee
    # switch to win
    s1_win_build.bat
    ./s2_pre_pack.coffee
    ./s3_build.coffee
    # see results in dist
    you can launch
    start_dist.bat
    ./start_dist.sh
    
    
    

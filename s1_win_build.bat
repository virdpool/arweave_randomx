cd src_c\

cd librandomx
call build.bat
cd ..

cd randomx_async\win
rd /s /q build
call npm run install
cd ..\..

cd hwloc\win
rd /s /q build
call npm run install
cd ..\..

cd ..


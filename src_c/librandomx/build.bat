mkdir build_win
cd build_win
REM "c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake" ..
"c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake" .. -A x64
REM "c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake" .. -A x86

"c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake" --build . --config Release
REM debug only
REM "c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe" RandomX.sln


cd ..

clear


if not exist build (
    mkdir build
)

set cpp_version=c++23

:: 源文件路径
set source_dir=src

:: 所有源文件
set source_files=%source_dir%\*.cpp %source_dir%\thread_pool\*.cpp

:: 输出文件名
set output_file=build\crest.exe

:: 包含目录
set include_dir=src

clang++ %source_files% bin\LLVM-C.lib -I%include_dir% -o %output_file% -std=%cpp_version% -g -DXOAOP_I128_SUPPORT -DCREST_DEBUG

cp build\crest.exe .\
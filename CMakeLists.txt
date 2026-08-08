cmake_minimum_required(VERSION 3.20)
project(Crest CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LLVM 库：Windows 用 LLVM-C.lib + 运行时 dll；Linux 用 libLLVM.so（LLVM-C API 内建其中）
if(WIN32)
    set(LLVM_C_LIB "${CMAKE_CURRENT_SOURCE_DIR}/bin/LLVM-C.lib")
else()
    set(LLVM_C_LIB "${CMAKE_CURRENT_SOURCE_DIR}/bin/libLLVM.so.21.1")
endif()

file(GLOB CREST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thread_pool/*.cpp"
)

add_executable(crest ${CREST_SOURCES})

target_include_directories(crest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# 所有配置：禁用异常 + i128 支持
target_compile_options(crest PRIVATE -fno-exceptions)
target_compile_definitions(crest PRIVATE XOAOP_I128_SUPPORT)

# Debug / Release 区分：
#   Debug   — 调试符号 + CREST_DEBUG 宏（DEBUG_LOG/ASSERT 等）
#   Release — 优化，关调试宏
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(crest PRIVATE -O2)
else()
    target_compile_options(crest PRIVATE -g)
    target_compile_definitions(crest PRIVATE CREST_DEBUG)
endif()

target_link_libraries(crest PRIVATE "${LLVM_C_LIB}")

# Windows：编译后把 LLVM-C.dll 复制到可执行文件同目录（运行依赖）
if(WIN32)
    add_custom_command(TARGET crest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/LLVM-C.dll"
            "$<TARGET_FILE_DIR:crest>/LLVM-C.dll")
endif()

# Linux：复制 .so 到项目根目录（与根目录 exe 同目录）；
# RPATH 指向 exe 自身目录（$ORIGIN）和 bin/，两处都能找 libLLVM.so.21.1。
# 用 BUILD_RPATH 属性而非 target_link_options：CMake 对 $ORIGIN 的转义在
# 链接选项中会变成 $$ORIGIN（字面路径），属性方式才正确
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET crest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/libLLVM.so.21.1"
            "${CMAKE_CURRENT_SOURCE_DIR}/libLLVM.so.21.1")
    set_target_properties(crest PROPERTIES
        BUILD_RPATH "$ORIGIN:${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif()

# 把可执行文件复制到项目根目录：编译器从自身目录（compiler_path）搜 std/ 等包，
# exe 必须和 std/ 同根，resolve_package_path("std/builtin") 才能命中
if(WIN32)
    set(CREST_ROOT_EXE "${CMAKE_CURRENT_SOURCE_DIR}/crest.exe")
else()
    set(CREST_ROOT_EXE "${CMAKE_CURRENT_SOURCE_DIR}/crest")
endif()
add_custom_command(TARGET crest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:crest>" "${CREST_ROOT_EXE}")
# Windows 根目录的 exe 运行依赖 LLVM-C.dll（根目录 dll 不跟踪，新克隆没有）
if(WIN32)
    add_custom_command(TARGET crest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/LLVM-C.dll"
            "${CMAKE_CURRENT_SOURCE_DIR}/LLVM-C.dll")
endif()

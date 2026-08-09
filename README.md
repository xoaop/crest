# Crest

A static-typed, GC-free compiled systems programming language.

## Build

Requires CMake (>= 3.20), Ninja, and Clang (support C++23).

Debug:

```
cmake -S . -B build
cmake --build build
```

Release:

```
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release
```

On Linux, use clang 21 with libc++:

```
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++"
cmake --build build
```

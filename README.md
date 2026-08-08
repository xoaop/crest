# Crest

A static-typed, GC-free compiled systems programming language.

## Build

Requires Clang (C++23) and LLVM 21 (C API library in `bin/`).

```bash
cmake -S . -B build
cmake --build build
```

On Linux, use clang 21 with libc++:

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++-21 -DCMAKE_CXX_FLAGS="-stdlib=libc++"
cmake --build build
```

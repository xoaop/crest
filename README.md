# Crest

Crest is a statically typed, garbage-collector-free, compiled systems programming
language with an LLVM backend.

```crest
main :: () -> i32 {
    return 0
}
```

Crest is in early development — the language and toolchain change frequently.

## Building from source

### Requirements

- **GCC 16 or newer.** The compiler is written in C++26 and uses static reflection
  (`-freflection`, P2996), which for now only GCC provides — Clang and MSVC cannot
  build it. On Windows, use MinGW-w64 (UCRT).
- **CMake 3.20 or newer** and **Ninja**.
- **LLVM 21.1** — the runtime library is bundled under `bin/` (`LLVM-C.lib` and
  `LLVM-C.dll` on Windows, `libLLVM.so.21.1` on Linux), so a separate LLVM
  installation is not required.

Crest uses git submodules, so clone recursively — or initialize them in an existing
clone:

```
git clone --recursive <repo-url>
git submodule update --init --recursive   # existing clone
```

### Windows

`build.bat` configures with g++, builds, and copies `crest.exe` and `LLVM-C.dll` to
the project root:

```
build.bat            # Debug (default)
build.bat release    # Release
```

### Linux

```
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++
cmake --build build
```

For a release build, add `-DCMAKE_BUILD_TYPE=Release` and use a separate build
directory (e.g. `build_release`).

The `crest` executable must run next to the LLVM runtime library and the bundled
`std/` directory; on Windows `build.bat` copies the executable to the project root
so it can find them.

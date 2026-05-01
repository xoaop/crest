# Crest 编程语言
静态类型、无 GC 的编译型系统编程语言，用于编写高性能底层软件。

## 特性
- 基于 LLVM 后端，生成优化机器码，性能与 C 相当
- 手动内存管理，无运行时额外开销
- 强类型系统，编译期类型检查
- 支持结构体、强类型枚举、函数局部类型定义
- 原生支持 C 互操作，可直接调用 C 标准库和系统 API
- 内置多线程编译，支持增量编译
- 编译期常量求值，零成本抽象
- 友好的错误提示，带精确源代码位置定位

## 快速开始
```cst
// demo.cst
import "std:c"

// 枚举定义
Color :: enum {
    Red;
    Green;
    Blue;
}

// 结构体定义
Point :: struct {
    x: i32;
    y: i32;
}

main :: () -> i32 {
    p := Point.{10, 20};
    color := Color.Green;
    
    c.printf("Point: (%d, %d), Color: %d\n".data, p.x, p.y, cast(i32)color);
    return 0;
}
```

```bash
# 编译（当前生成 .o 目标文件，链接器开发中）
crest build demo.cst
```

## 项目架构
```
前端：词法分析 → 语法分析 → AST 生成
中端：语义分析 → 类型检查 → 常量折叠 → 符号解析
后端：LLVM IR 生成 → 优化 → 目标文件生成
```

### 核心模块
| 模块 | 功能 |
|------|------|
| xoaop.h | 基础库：内存分配器、字符串、容器等 |
| tokenizer/parser | 词法/语法分析 |
| analyser/type_check | 语义分析与类型检查 |
| evaluator | 编译期常量求值 |
| package | 包管理与依赖解析 |
| llvm_generate_ir | LLVM 代码生成 |

## 开发进度
- 核心功能完成度 70%
- 已实现完整的前端、中端和 LLVM 后端
- 正在开发链接器、字符串类型、Union 类型等特性
- 规划中：泛型、内存安全检查、标准库、LSP 支持

## 构建
### 依赖
- Clang 16+ / GCC 13+ / MSVC 2022 17.4+ (C++23 支持)
- LLVM 17+ 开发库

### 编译
```bash
# Windows
build.bat

# Linux
./build_linux.bat
```

## 语法示例
### 变量声明
```cst
// 编译期常量
PI :: 3.14159

// 自动类型推导
a := 42

// 显式指定类型
b: i64 = 100
```

### 函数
```cst
add :: (a: i32, b: i32) -> i32 {
    return a + b;
}
```

## 许可证
MIT License

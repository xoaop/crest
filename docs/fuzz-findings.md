# fuzz 摸底：深嵌套极限与崩溃根因

> 2026-11-14。方法：对每种声明形式按深度递增（64 → 16384）生成源码，跑 `crest build`，记录第一个非 ok 的结局。
> 判据：正常成功或正常报错（退出码 0/1）算 ok；NTSTATUS、LLVM OOM、超时都算崩溃。

## 一、各形式的极限

| 形式 | 生成内容 | 最大 ok | 崩溃点 | 失败方式 |
|---|---|---|---|---|
| `nested_fn` | 函数体内嵌套函数，每层 `return F();` | 16 | 64 | 正常报错（RecursionGuard 100 层） |
| `const_chain` | `A0 :: A1; A1 :: A2; ...; An :: 1` | 2048 | 4096 | `STACK_OVERFLOW` |
| `if_nest` | 嵌套 `#if` | 4096 | 8192 | `STACK_OVERFLOW` |
| `mixed_ptr` | `struct { e: E; u: U; p: *前一个; n: i32; }` 链 | 4096 | 8192 | `ASSERT type.cpp:871: add_type_unique: insert failed` |
| `local_structs` | 函数体内 `struct` 链 | 8192 | 16384 | 同上（类型表满） |
| `type_ctor` | `Box :: $(T: type) -> type`，`B1 :: Box(B0)` 链 | 8192 | 16384 | 同上（类型表满） |
| `mixed_agg` | `struct { arr: [2]前一个; }` 链 | 12 | 16 | `ACCESS_VIOLATION`；≥20 `LLVM ERROR: out of memory` |

除 `nested_fn` 外全部崩溃，没有一条走到正常报错。

## 二、四个根因

### 1. 类型表固定容量 16384

`src/type.cpp:871` 的 `add_type_unique` 在表满时 `ASSERT_MSG("insert failed")`。
`local_structs` / `type_ctor` / `mixed_ptr` 撞的都是这一条。

### 2. 递归遍历没有统一深度保护

`STACK_OVERFLOW`（0xC00000FD）出现在 AST 遍历、CIR 构建、常量递归求值。
`RecursionGuard`（`MAX_RECURSION_DEPTH = 100`）只覆盖 parser 的语句/表达式路径，
`nested_fn` 因此在 64 层就被拦住并正常报错，其它路径没有等价保护。

### 3. 聚合大小指数增长无上限

```crest
X0 :: struct { arr: [2]i32; }
X1 :: struct { arr: [2]X0; }
...
```

每层 `[2]` 让大小翻倍。12 层 ok，16 层 `ACCESS_VIOLATION`，20 层直接
`LLVM ERROR: out of memory` + `Allocation failed`（fail-fast）。
`type_size_of` 没有溢出检查，也没有「类型过大」的上限。

### 4. `BadExpr` 进入 CIR builder 后 `UNREACHABLE()`

```crest
#if
```

```
[DEBUG] cir_builder.cpp:1422: Unsupported AST type for CIR generation: bad expr
Assert FAILED at cir_builder.cpp:1423: unreachable
```

parser 用 `BadExpr` / `BadStmt` / `BadType` 做错误恢复，但
`build_inst_for_expr` 的 default 分支直接 `UNREACHABLE()`。
随机变异（truncate / mutate_tok）里所有 `ILLEGAL_INSTRUCTION (ud2)` 都是这一处。

## 三、修复方向

1. `type_size_of`：数组 `elem * count`、结构体累加加溢出检查，超限报「类型过大」。
2. 类型表：满时给出可读诊断（或改为可增长表）。
3. 递归遍历：把 `RecursionGuard` 推广到 AST 遍历 / CIR 构建 / 常量求值。
4. `build_inst_for_expr`：`BadExpr`/`BadStmt`/`BadType` 返回错误占位指令，不 `UNREACHABLE`。

## 四、fuzz 覆盖现状（test/fuzz.py）

recipe：`nest` `truncate` `mutate_tok` `mutate_byte` `numbers` `graph`
`comptime` `anon` `global` `decl` `degenerate`。

本表用 `decl` recipe 的六种形式加探针脚本得到；`global` recipe 覆盖
文件作用域变量（i32/u8/i64/f64/bool/数组/字符串/枚举/null 指针/含 string 字段的
struct/匿名结构体 + 缺初值/`---`/类型不匹配/运行期初值 + 缺 directive）。

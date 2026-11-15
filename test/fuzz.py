"""
Crest compiler robustness fuzzer.

Invariant under test (see oracle.py):

    For ANY input the compiler must either compile it, or reject it with a
    human-readable diagnostic.  It must never crash.

This is a *differential* kind of testing: it does not check that valid
programs compile (that is run.py's job) — it hunts for inputs that make
the compiler die.  There is no "correct" output for a malformed program;
the only observable failure is abnormal termination.

Corpus recipes
--------------
nest       纯 token 深嵌套（( { [ * + - . 泛型 <>），size 可调
truncate   把 pass/ 与 fuzz_corpus/ 里的文件从 1/16..16/16 处截断
mutate_tok 删/换/复制 token
mutate_byte 翻位、插 NUL、高位字节、非法 UTF-8、裸 CR
numbers    数字与类型名边界值
graph      import 链/环、自引用 struct、深泛型递归
comptime   编译期函数 / 嵌套闭包捕获 / -> ? 推导；含失调用的畸形变体
anon       匿名结构体字面量：具名/位置/空/嵌套/畸形
degenerate 空、纯空白、纯注释、BOM

Reproducibility
---------------
Every case is generated from a per-case `random.Random(seed)` — never the
module-level `random`.  Each generated file carries a provenance header:

    // fuzz: recipe=<name> seed=<seed> size=<n> [src=<path>]

so a crash can be replayed by regenerating that one case.  A fuzzer whose
crashes cannot be reproduced is worthless, so this is not optional.

Usage
-----
    python test/fuzz.py                 # quick pass  (~300 cases)
    python test/fuzz.py --deep          # ~3000 cases
    python test/fuzz.py --recipe nest   # a single recipe
    python test/fuzz.py --seed 7        # different corpus
    python test/fuzz.py --replay 12     # regenerate+run case #12 only
"""

import argparse
import os
import pathlib
import random
import re
import shutil
import sys

import oracle

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(SCRIPT_DIR)
FUZZ_DIR = os.path.join(SCRIPT_DIR, "build_fuzz")
CORPUS_DIR = pathlib.Path(SCRIPT_DIR) / "fuzz_corpus"
PASS_DIR = pathlib.Path(SCRIPT_DIR) / "pass"

TIMEOUT = 10.0          # a compiler that needs >10s on a fuzz case is hanging
DEFAULT_SEED = 20260913


# ------------------------------------------------------------------ helpers

def chunks(n, k):
    """Split [0, n) into k roughly equal parts — used for truncate offsets."""
    if k <= 0:
        return []
    step = max(1, n // k)
    return list(range(1, n, step))


def provenance(recipe, seed, size, src=None):
    parts = [f"recipe={recipe}", f"seed={seed}", f"size={size}"]
    if src:
        parts.append(f"src={src}")
    return "// fuzz: " + " ".join(parts) + "\n"


def blank_lines(text):
    """Line indices that contain only whitespace."""
    return [i for i, ln in enumerate(text.splitlines()) if not ln.strip()]


def split_into_line_groups(text, group_size):
    """Cut `text` at blank-line boundaries into ~equal line groups.

    Used to synthesise single-file imports: a group of top-level
    declarations becomes its own file so that `import "name"` (the
    check_file_legal path in resolve_package_path) gets exercised —
    the `pass/` and `fuzz_corpus/` seeds are directories, so that path
    is otherwise never hit.
    """
    if group_size <= 0:
        return None
    lines = text.splitlines(keepends=True)
    if len(lines) < group_size * 2:
        return None
    cut = blank_lines(text)
    cut = [i for i in cut if group_size <= i <= len(lines) - group_size]
    if not cut:
        return None
    k = max(1, len(lines) // group_size)
    step = max(1, len(cut) // k)
    groups, prev = [], 0
    for i in cut[::step]:
        groups.append("".join(lines[prev:i + 1]))
        prev = i + 1
    groups.append("".join(lines[prev:]))
    groups = [g for g in groups if g.strip()]
    return groups if len(groups) >= 2 else None


def seed_sources():
    """All files the mutation recipes draw from: fuzz_corpus + pass."""
    out = []
    for d in (CORPUS_DIR, PASS_DIR):
        if d.is_dir():
            out.extend(sorted(p for p in d.rglob("*.cst")))
    return out


# --------------------------------------------------------------- recipes

def g_nest(rng, size):
    """Pure-token deep nesting.  size = bracket depth."""
    shapes = {
        "paren":  ("(", ")"),
        "brace":  ("{", "}"),
        "brack":  ("[", "]"),
        "star":   ("*", ""),      # pointer depth
        "plus":   ("1 + ", ""),
        "minus":  ("-", ""),
        "tilde":  ("~", ""),
        "dot":    ("a.", ""),
    }
    name = rng.choice(sorted(shapes))
    open_s, close_s = shapes[name]
    body = open_s * size
    if close_s:
        body += "1" + close_s * size
    else:
        body += "1" if name == "star" else "x"
        if name == "minus":
            body += "1"

    return (
        provenance("nest", 0, size) +
        f"// shape={name}\n"
        f"main :: () -> i32 {{\n"
        f"    x := {body};\n"
        f"    return 0;\n"
        f"}}\n"
    )


def g_truncate(rng, src_path):
    """Cut a real source file at a byte offset — the classic parser crasher."""
    text = src_path.read_text(encoding="utf-8", errors="replace")
    offsets = chunks(len(text), 16)
    if not offsets:
        return None
    off = rng.choice(offsets)
    body = text[:off]
    # Cut mid-identifier / mid-string often enough to matter.
    if rng.random() < 0.3 and off < len(text):
        body = text[:off + rng.randint(1, 8)]
    return provenance("truncate", 0, off, src=src_path.name) + body


def g_mutate_tok(rng, src_path):
    """Delete / duplicate / replace tokens."""
    text = src_path.read_text(encoding="utf-8", errors="replace")
    toks = re.findall(r"\S+|\s+", text)
    if len(toks) < 4:
        return None
    n_edits = rng.randint(1, 6)
    for _ in range(n_edits):
        if not toks:
            break
        i = rng.randrange(len(toks))
        op = rng.choice(("del", "dup", "rep"))
        if op == "del":
            del toks[i]
        elif op == "dup":
            toks.insert(i, toks[i])
        else:
            toks[i] = rng.choice([
                "(", ")", "{", "}", "[", "]", ";", ",", ":", "::", ":=",
                "->", "$", "$(", "?", "*", "&", "|", "~", "<<", ">>",
                ".{", "@", "#if", "#under_is_file_scope",
                "0", "1e999",
                "struct", "enum", "union", "for", "if", "else", "return",
                "\x00", "🐟",
            ])
    return provenance("mutate_tok", 0, n_edits, src=src_path.name) + "".join(toks)


def g_mutate_byte(rng, src_path):
    """Bit flips, NULs, high bytes, invalid UTF-8, bare CR."""
    data = bytearray(src_path.read_bytes())
    if not data:
        return None
    n_edits = rng.randint(1, 8)
    for _ in range(n_edits):
        i = rng.randrange(len(data))
        op = rng.choice(("flip", "nul", "high", "cr", "trunc_utf8", "ins"))
        if op == "flip":
            data[i] ^= 1 << rng.randrange(8)
        elif op == "nul":
            data[i] = 0
        elif op == "high":
            data[i] = rng.randrange(0x80, 0x100)
        elif op == "cr":
            data[i] = 0x0D
        elif op == "trunc_utf8":
            # cut a multi-byte sequence in half: 0xC2..0xF4 followed by ASCII
            data[i] = rng.randrange(0xC2, 0xF5)
            if i + 1 < len(data):
                data[i + 1] = ord('A')
        else:
            data.insert(i, rng.randrange(0, 0x100))
    # Decode back to text for the driver; surrogateescape keeps bad bytes.
    body = bytes(data).decode("utf-8", errors="surrogateescape")
    return provenance("mutate_byte", 0, n_edits, src=src_path.name) + body


def g_numbers(rng, size):
    """Numeric literal and type-name edge cases."""
    cases = [
        "x := 1" + "0" * 10000 + ";",
        "x := 1e999999;",
        "x := 1e-999999;",
        "x := 0x;",
        "x := 0x" + "F" * 100 + ";",
        "x := 0b;",
        "x := 0o;",
        "x := 0.;",
        "x := .5;",
        "x := 1..2;",
        "x := -0;",
        "x := -0.0;",
        "x := 99999999999999999999999999999999;",
        "x := 1i0;",
        "x := 1i999999;",
        "x := 1u0;",
        "x := 1f0;",
        "x := 99999999999999999999i32;",
        "x := 1 << 999999999;",
        "x := 1 / 0;",
        "x := 1 % 0;",
    ]
    pick = rng.sample(cases, k=min(size or 4, len(cases)))
    body = "\n".join("    " + c for c in pick)
    return (
        provenance("numbers", 0, size) +
        f"main :: () -> i32 {{\n{body}\n    return 0;\n}}\n"
    )


def g_comptime(rng, size):
    """编译期函数 / 嵌套闭包 / 实例链形态，size = 嵌套深度。

    生成 `$()` 函数返回若干层闭包，自由变量分别取 包级 const / 局部 const /
    局部变量 / 形参；再叠加少量畸形调用（参数个数、非结构体实参），
    覆盖实例链查找与错误路径。
    """
    depth = rng.choice([1, 1, 1, 2, 3])
    cap = rng.choice(["const", "local", "var", "param"])
    expr = rng.choice(["X", "n", "X + n"]) if rng.random() < 0.3 else "X"

    src = "Wrap :: $(n: i32) -> ? {\n"
    if cap == "const":
        src += "    X :: n * 2;\n"
    elif cap == "local":
        src += "    X : i32 = n * 2;\n"
    elif cap == "var":
        src += "    X : i32 = ---;\n    X = n * 2;\n"
    else:
        expr = "n"

    ind = "    "
    for d in range(depth):
        rt = "i32" if d == depth - 1 else "?"
        src += f"{ind}return (v: i32) -> {rt} {{\n"
        ind += "    "
    src += f"{ind}return {expr} + v;\n"
    for _ in range(depth):
        ind = ind[:-4]
        src += f"{ind}}};\n"
    src += "}\n\n"

    args = "".join(f"({i + 2})" for i in range(depth))
    call = f"Wrap(1){args}"
    stmt = f"    {call};\n"
    if rng.random() < 0.25:
        # 畸形调用：数量不对、缺实参、拿结构体当实参
        stmt = "    " + rng.choice([
            "Wrap()",
            "Wrap(1)",
            "Wrap(1)(2)(3)(4)",
            "Wrap(1)(.{ 2 })",
        ]) + ";\n"

    return (
        provenance("comptime", 0, depth) +
        src +
        f"main :: () -> i32 {{\n{stmt}    return 0;\n}}\n"
    )


def g_anon(rng, size):
    """匿名结构体字面量：具名 / 位置 / 空 / 嵌套 / 畸形。"""
    forms = [
        ".{ 1, 2 }",
        ".{ }",
        ".{ a = 1, b = 2 }",
        ".{ .{ 1, 2 }, 3 }",
        ".{ 1, b = 2 }",                 # 混用具名与位置
        ".{ a = 1, a = 2 }",             # 字段名重复
        ".{ 1, \"x\", true }",
        ".{ 99999999999999999999999 }",  # 溢出
    ]
    pick = rng.choice(forms)
    uses = [
        f"v := {pick};\n    return cast(i32)field_count(typeof(v));",
        f"v := {pick};\n    return get_field(v, 0);",
        f"v := {pick};\n    return get_field(v, 99);",     # 越界
        f"v := {pick};\n    return 0;",
    ]
    use = rng.choice(uses)

    if rng.random() < 0.4:
        use = rng.choice([
            "p : Point\n    p = Point.{ 1, 2 }\n    return p.x;",
            "p := Point.{ z = 1 }\n    return 0;",        # 未知字段
            "p := Point.{ 1, 2, 3 }\n    return 0;",      # 数量不匹配
        ])

    return (
        provenance("anon", 0, size) +
        "Point :: struct { x: i32; y: i32; }\n\n"
        f"main :: () -> i32 {{\n    {use}\n}}\n"
    )


def g_global(rng, size):
    """全局变量：文件作用域、编译期常量初值、字符串/聚合/指针/畸形变体。"""
    prelude = (
        "Color :: enum { RED; GREEN; }\n"
        "S :: struct { name: string; n: i32; }\n"
        "make :: () -> i32 { return 1; }\n\n"
    )

    decls = [
        ("i32v : i32 = 5", ["i32v = i32v + 1;", "if i32v != 5 { return 1; }"]),
        ("u8v : u8 = 200", ["if u8v != 200 { return 1; }"]),
        ("i64v : i64 = 1i64", ["i64v = i64v - 1;"]),
        ("f := 2.5", ["if f < 2.4 { return 1; }"]),
        ("b := true", ["if !b { return 1; }"]),
        ("arr : [3]i32 = { 1, 2, 3 }", ["if arr[0] != 1 { return 1; }"]),
        ("s := \"hi\"", ["if s.count != 2 { return 1; }", "if s[0] != 104 { return 2; }"]),
        ("e : Color = Color.RED", ["if e != Color.RED { return 1; }"]),
        ("p : *i32 = null", ["if p != null { return 1; }"]),
        ("nested : S = .{ \"x\", 1 }",
         ["if nested.n != 1 { return 1; }", "if nested.name.count != 1 { return 2; }"]),
        ("anon := .{ a = 1, b = \"z\" }", ["if anon.a != 1 { return 1; }"]),
    ]

    picked = rng.sample(decls, k=rng.randint(1, 4))
    body = "\n".join(d for d, _ in picked)
    uses = "\n    ".join(rng.choice(u) for _, u in picked)

    # 畸形变体：缺初值 / --- / 类型不匹配 / 运行期初值
    if rng.random() < 0.25:
        body = rng.choice([
            "noinit : i32;",
            "zeroed : i32 = ---;",
            "mismatch : i32 = \"str\";",
            "runtime : i32 = make();",
        ])
        uses = ""

    # 偶发省略 directive，覆盖「必须文件作用域」的错误路径
    scope = "" if rng.random() < 0.1 else "#under_is_file_scope\n"

    return (
        provenance("global", 0, size) +
        prelude + scope +
        body + "\n\n" +
        f"main :: () -> i32 {{\n    {uses}\n    return 0;\n}}\n"
    )


def g_graph(rng, size):
    """Package graph / self-reference pathology.  size = chain length.

    Returns (main_text, extra_files).  Crest `import` names a *directory*
    (see resolve_package_path → check_directory_legel), so a package is
    `<dir>/<name>/<name>.cst` and the import string is the directory name.

    A rarer third shape covers the single-file import path
    (check_file_legal) by slicing a seed file into named line groups.
    """
    n = size or 4
    roll = rng.random()

    if roll < 0.15:
        # Single-file import.  Crest `import` is BOTH a path and a namespace
        # (see test/pass/generic_pkg: `import "foo"` → call as `foo.b()`),
        # so a bare file works when the import string ends in `.cst`.
        #
        # Groups containing `main` stay in the top-level file — two
        # definitions of main across files would produce an ordinary
        # duplicate-symbol error that drowns out real crashes.
        srcs = seed_sources()
        src = rng.choice(srcs) if srcs else None
        if src:
            groups = split_into_line_groups(
                src.read_text(encoding="utf-8", errors="replace"), 8)
            if groups:
                top = [g for g in groups if "main ::" in g]
                rest = [g for g in groups if "main ::" not in g]
                if rest:
                    extras = {f"part{i}.cst": g for i, g in enumerate(rest)}
                    imports = "".join(f'import "part{i}.cst"\n' for i in range(len(rest)))
                    main = (provenance("graph", 0, len(rest), src=src.name) +
                            imports + "".join(top))
                    return main, extras

    if rng.random() < 0.5:
        # import chain: main -> g1 -> g2 -> ... -> gN -> (g1 | missing)
        extras = {}
        for i in range(1, n + 1):
            if i < n:
                nxt = f"g{i+1}"
            else:
                nxt = rng.choice(["g1", "g999"]) if rng.random() < 0.5 else None
            head = f'import "{nxt}"\n' if nxt else ""
            extras[f"g{i}/{i}.cst"] = head + f"v{i} :: {i};\n"
        main = (provenance("graph", 0, n) +
                'import "g1"\n'
                "main :: () -> i32 { return 0; }\n")
        return main, extras
    else:
        # deep self-referential struct chain (by value / by pointer)
        s = "".join(
            (f"S{i} :: struct {{ inner: *S{i+1}; }};\n" if rng.random() < 0.5
             else f"S{i} :: struct {{ inner: S{i+1}; }};\n")
            for i in range(n)
        )
        s += f"S{n} :: struct {{ v: i32; }};\n"
        s += "main :: () -> i32 { return 0; }\n"
        return provenance("graph", 0, n) + s, {}


def g_decl(rng, size):
    """声明形式的嵌套：局部结构体链、嵌套函数、#if 嵌套、编译期类型构造、常量链、混合聚合。"""
    depth = max(1, size)

    def local_structs(d):
        decls = "\n".join(
            f"    S{i+1} :: struct {{ v: {'i32' if i == 0 else f'S{i}'}; }};"
            for i in range(d))
        return ("main :: () -> i32 {\n" + decls +
                "\n    a := S1.{ 1 };\n    return 0;\n}\n")

    def nested_fn(d):
        lines, ind = [], "    "
        for i in range(d):
            lines.append(f"{ind}F{i} :: () -> i32 {{")
            ind += "    "
        lines.append(f"{ind}return 0;")
        for _ in range(d):
            ind = ind[:-4]
            lines.append(f"{ind}}}")
        return ("main :: () -> i32 {\n" + "\n".join(lines) +
                "\n    return 0;\n}\n")

    def if_nest(d):
        lines, ind = [], ""
        for _ in range(d):
            lines.append(f"{ind}#if true {{")
            ind += "    "
        lines.append(f"{ind}A :: 1")
        for i in range(d):
            ind = ind[:-4]
            lines.append(f"{ind}}} else {{")
            lines.append(f"{ind}    A :: {i + 2}")
            lines.append(f"{ind}}}")
        return "\n".join(lines) + "\n\nmain :: () -> i32 { return 0; }\n"

    def type_ctor(d):
        s = "Box :: $(T: type) -> type { return struct { inner: T; } }\n"
        cur = "i32"
        for i in range(d):
            s += f"B{i+1} :: Box({cur})\n"
            cur = f"B{i+1}"
        s += f"\nmain :: () -> i32 {{\n    v : {cur};\n    return 0;\n}}\n"
        return s

    def const_chain(d):
        s = "".join(f"A{i} :: A{i+1}\n" for i in range(d))
        s += f"A{d} :: 1\n"
        if rng.random() < 0.3:
            s += f"A{d+1} :: A0\n"        # 循环引用
        s += "\nmain :: () -> i32 { return A0; }\n"
        return s

    def mixed_agg(d):
        s = ("E :: enum { A; B; }\n"
             "U :: union { i: i32; f: f32; }\n")
        cur = "i32"
        for i in range(d):
            s += f"X{i} :: struct {{ e: E; u: U; arr: [2]{cur}; n: i32; }}\n"
            cur = f"X{i}"
        s += f"\nmain :: () -> i32 {{\n    v : {cur};\n    return 0;\n}}\n"
        return s

    forms = [local_structs, nested_fn, if_nest, type_ctor, const_chain, mixed_agg]
    text = rng.choice(forms)(depth)

    return provenance("decl", 0, depth) + text


def g_degenerate(rng, size):
    """Empty / whitespace / comment-only / BOM files."""
    cases = [
        "",
        "\n",
        " " * 1000,
        "\t\r\n" * 100,
        "// no newline at end",
        "//",
        "/* unterminated block comment",
        "/*" + "*/" * 50,
        "﻿",
        "﻿main :: () -> i32 { return 0; }\n",
        "\x00" * 64,
        "\r" * 64,
        "\\" ,
    ]
    pick = cases[rng.randrange(len(cases))]
    return provenance("degenerate", 0, size) + pick


# ------------------------------------------------------------- case model

class Case:
    def __init__(self, idx, recipe, seed, size, text, extra_files=None):
        self.idx = idx
        self.recipe = recipe
        self.seed = seed
        self.size = size
        self.text = text
        self.extra_files = extra_files or {}

    def label(self):
        return f"#{self.idx} {self.recipe} seed={self.seed} size={self.size}"

    def write(self, directory):
        """Materialise on disk.  Multi-file cases get a subdirectory."""
        if self.extra_files:
            d = os.path.join(directory, f"case_{self.idx}")
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "main.cst"), "w", encoding="utf-8",
                      errors="surrogateescape", newline="\n") as f:
                f.write(self.text)
            for rel, content in self.extra_files.items():
                sub = os.path.dirname(rel)
                if sub:
                    os.makedirs(os.path.join(d, sub), exist_ok=True)
                with open(os.path.join(d, rel), "w", encoding="utf-8",
                          errors="surrogateescape", newline="\n") as f:
                    f.write(content)
            return d
        p = os.path.join(directory, f"case_{self.idx}.cst")
        with open(p, "w", encoding="utf-8", errors="surrogateescape",
                  newline="\n") as f:
            f.write(self.text)
        return p


RECIPES = ("nest", "truncate", "mutate_tok", "mutate_byte",
           "numbers", "graph", "comptime", "anon", "global", "decl", "degenerate")

# nest sizes are deliberately clustered around the observed cliff
NEST_SIZES = [8, 16, 24, 32, 48, 64, 100, 128, 200, 256, 512, 1024, 5000]
# decl nesting depth: enough to reach parser/analyser stack limits
DECL_SIZES = [8, 16, 32, 64, 100, 128, 256, 512, 1024, 2048]


def generate(seed, counts, recipes):
    """Build the case list.  Deterministic for a given (seed, counts, recipes)."""
    cases = []
    srcs = seed_sources()
    idx = 0

    for recipe in recipes:
        n = counts.get(recipe, 0)
        for k in range(n):
            case_seed = seed * 100003 + idx
            rng = random.Random(case_seed)
            if recipe == "nest":
                size = NEST_SIZES[k % len(NEST_SIZES)]
            elif recipe == "decl":
                size = DECL_SIZES[k % len(DECL_SIZES)]
            else:
                size = rng.choice([2, 4, 8, 16, 32, 64])
            src = rng.choice(srcs) if srcs else None

            if recipe == "nest":
                text, extra = g_nest(rng, size), None
            elif recipe == "truncate" and src:
                text, extra = g_truncate(rng, src), None
            elif recipe == "mutate_tok" and src:
                text, extra = g_mutate_tok(rng, src), None
            elif recipe == "mutate_byte" and src:
                text, extra = g_mutate_byte(rng, src), None
            elif recipe == "numbers":
                text, extra = g_numbers(rng, size), None
            elif recipe == "graph":
                text, extra = g_graph(rng, size)
            elif recipe == "comptime":
                text, extra = g_comptime(rng, size), None
            elif recipe == "anon":
                text, extra = g_anon(rng, size), None
            elif recipe == "global":
                text, extra = g_global(rng, size), None
            elif recipe == "decl":
                text, extra = g_decl(rng, size), None
            elif recipe == "degenerate":
                text, extra = g_degenerate(rng, size), None
            else:
                continue

            if text is None:
                continue

            # Stamp the real per-case seed into the provenance line.
            text = re.sub(r"seed=\d+", f"seed={case_seed}", text, count=1)
            cases.append(Case(idx, recipe, case_seed, size, text, extra))
            idx += 1

    return cases


# ------------------------------------------------------------------- main

QUICK_COUNTS = {"nest": 60, "truncate": 60, "mutate_tok": 50,
                "mutate_byte": 50, "numbers": 25, "graph": 25,
                "comptime": 40, "anon": 30, "global": 30, "decl": 30,
                "degenerate": 30}
DEEP_COUNTS = {"nest": 300, "truncate": 700, "mutate_tok": 500,
               "mutate_byte": 500, "numbers": 200, "graph": 300,
               "comptime": 400, "anon": 300, "global": 300, "decl": 300,
               "degenerate": 200}


def main():
    ap = argparse.ArgumentParser(description="Crest compiler robustness fuzzer")
    ap.add_argument("--deep", action="store_true", help="larger corpus (~3000 cases)")
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--recipe", action="append", choices=RECIPES,
                    help="only this recipe (repeatable)")
    ap.add_argument("--replay", type=int, metavar="N",
                    help="regenerate and run only case #N")
    ap.add_argument("--max-report", type=int, default=30,
                    help="stop after this many crashes (0 = no limit)")
    ap.add_argument("--keep", action="store_true",
                    help="keep generated cases (default: only crashing ones)")
    args = ap.parse_args()

    crest = oracle.default_crest()
    if not os.path.exists(crest):
        print(f"error: {crest} not found in project root")
        return 1

    shutil.rmtree(FUZZ_DIR, ignore_errors=True)
    os.makedirs(FUZZ_DIR, exist_ok=True)

    recipes = args.recipe or list(RECIPES)
    counts = DEEP_COUNTS if args.deep else QUICK_COUNTS
    cases = generate(args.seed, counts, recipes)

    if args.replay is not None:
        cases = [c for c in cases if c.idx == args.replay]
        if not cases:
            print(f"error: no case #{args.replay} in this corpus "
                  f"(seed={args.seed}, recipes={recipes})")
            return 1

    print(f"=== Crest fuzz  |  seed={args.seed}  cases={len(cases)}  "
          f"recipes={','.join(recipes)} ===")
    if args.replay is None:
        print(f"    binary: {crest}")
    print()

    crashes = []
    hangs = []
    per_recipe = {}

    for c in cases:
        path = c.write(FUZZ_DIR)
        rc, out, timed_out = oracle.run_compiler(
            crest, oracle.build_args(path, FUZZ_DIR), timeout=TIMEOUT)
        verdict, reason = oracle.judge(rc, out, timed_out)

        st = per_recipe.setdefault(c.recipe, {"ok": 0, "crash": 0, "hang": 0})
        if verdict == oracle.OK:
            st["ok"] += 1
        elif verdict == oracle.HANG:
            st["hang"] += 1
            hangs.append((c, reason, out))
            print(f"  [HANG] {c.label()}")
        else:
            st["crash"] += 1
            crashes.append((c, reason, out))
            print(f"  [CRASH] {c.label()}  -> {reason}")

        if args.replay is not None:
            print()
            print(f"--- full output for case #{c.idx} ---")
            print(out if out else "(no output)")

        if not args.keep and verdict == oracle.OK:
            # Keep the tree small: drop clean cases as we go.
            if os.path.isdir(path):
                shutil.rmtree(path, ignore_errors=True)
            elif os.path.exists(path):
                os.remove(path)

        if args.max_report and len(crashes) >= args.max_report:
            print(f"\n  (stopping: reached --max-report={args.max_report})")
            break

    print()
    print("--- per recipe ---")
    for r in recipes:
        st = per_recipe.get(r, {"ok": 0, "crash": 0, "hang": 0})
        flag = "  <-- BAD" if (st["crash"] or st["hang"]) else ""
        print(f"  {r:<12} ok={st['ok']:<5} crash={st['crash']:<4} hang={st['hang']}{flag}")

    print()
    print(f"  {len(crashes)} crash, {len(hangs)} hang, "
          f"{sum(s['ok'] for s in per_recipe.values())} clean")

    if crashes or hangs:
        print()
        print("--- reproducible cases (kept in test/build_fuzz/) ---")
        for c, reason, _ in crashes[:20]:
            print(f"  #{c.idx:<5} {c.recipe:<12} {reason}")
        print()
        print("  replay one with:  python test/fuzz.py "
              f"--seed {args.seed} --replay <N>")
        return 1

    if not args.keep:
        shutil.rmtree(FUZZ_DIR, ignore_errors=True)
    print()
    print("  no crashes — the compiler survived every generated input.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

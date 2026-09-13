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
                "->", "$", "*", "&", "|", "~", "<<", ">>", "0", "1e999",
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
           "numbers", "graph", "degenerate")

# nest sizes are deliberately clustered around the observed cliff
NEST_SIZES = [8, 16, 24, 32, 48, 64, 100, 128, 200, 256, 512, 1024, 5000]


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
            size = NEST_SIZES[k % len(NEST_SIZES)] if recipe == "nest" \
                else rng.choice([2, 4, 8, 16, 32, 64])
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
                "mutate_byte": 50, "numbers": 25, "graph": 25, "degenerate": 30}
DEEP_COUNTS = {"nest": 300, "truncate": 700, "mutate_tok": 500,
               "mutate_byte": 500, "numbers": 200, "graph": 300, "degenerate": 200}


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

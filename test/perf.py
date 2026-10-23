"""
Crest 编译期性能回归测试（不是计时基准，是"复杂度"回归）。

为什么用比值而不是绝对时间
------------------------------------------------
绝对秒数换台机器就失效；能稳定回归的是**同一台机器上的比值**：

  1. suffix_scan    同样字节数的注释放在文件末尾 vs 放在开头。
                    两者 token 数相同、总字节相同，唯一差别是"注释在
                    token 之后还是之前"。曾经 xp_make_string_count 内部
                    会对每个 token 做 strlen(到缓冲区末尾)，于是尾部
                    注释给每个 token 都加了 300KB 的扫描量 —— 比值 2.6。
                    现在应该 ≈ 1.0。
  2. linear_scaling N 个函数 vs 2N 个函数。线性 ≈ 2.0；任何 O(n^2) 项
                    会把它推向 4.0。

阈值留了宽余量（1.8 / 3.0），只用来抓"复杂度退化"，不抓 10% 的抖动。

阈值怎么来的（修复前 / 修复后实测，同一台机器）
------------------------------------------------
    项目                  修复前      修复后    limit
    6400 函数            6.48s      1.40s      —
    6400 函数 + 尾部注释 18.89s      1.34s      —
    tail / head (6400)   2.67        1.00       1.8
    tail / head (4000)   ~2.6        1.01       1.8
    8k / 4k 函数         ~3.4        1.95       3.0

即阈值都落在"退化"和"正常"中间，且离正常值有 ~1.8x 余量。

用法
------------------------------------------------
    python test/perf.py
    python test/perf.py --quick        # 用例规模减半
    CREST_PERF_DIR=<可写目录> python test/perf.py

输出目录默认 test/build_perf；该目录写不进去时用 CREST_PERF_DIR 指定。
"""

import os
import pathlib
import sys
import time

import oracle

CREST = oracle.default_crest()

PERF_DIR = pathlib.Path(os.environ.get("CREST_PERF_DIR", "test/build_perf"))

# 每个用例取 BEST_OF 次里最快的一次，压掉调度噪声
BEST_OF = 3
TIMEOUT = 180

# 阈值（见文件头说明）
SUFFIX_RATIO_LIMIT = 1.8
SCALING_RATIO_LIMIT = 3.0

# 注释体积：300KB 足够让"每 token 扫到末尾"的退化暴露出来，
# 又不至于让整个测试跑太久
BIG_COMMENT_BYTES = 300 * 1024


def gen_functions(n):
    """n 个平凡函数 + 一个 main。token 数与该函数的文本长度都稳定。"""
    lines = [f"f{i} :: (a: i32) -> i32 {{ return a; }}" for i in range(n)]
    lines.append("main :: () -> i32 { return 0; }")
    return "\n".join(lines) + "\n"


def write_case(name, text):
    path = PERF_DIR / f"{name}.cst"
    path.write_text(text, encoding="utf-8")
    return path


def time_compile(src):
    """编译一次，返回 (最快秒数, 错误字符串)。错误字符串非空表示编译失败。"""
    best = None
    last_error = ""

    for _ in range(BEST_OF):
        t0 = time.perf_counter()
        rc, out, timed_out = oracle.run_compiler(
            CREST, ["build", str(src), "-c", "-o", str(PERF_DIR)], timeout=TIMEOUT)
        elapsed = time.perf_counter() - t0

        verdict, reason = oracle.judge(rc, out, timed_out)
        if verdict != oracle.OK:
            return None, f"compiler {verdict.lower()}: {reason}"

        if rc != 0:
            return None, f"exit {rc}\n{oracle.tail(out, 400)}"

        n_errors = sum(1 for line in out.splitlines() if "error(s)" in line)
        if n_errors:
            return None, f"compile reported errors\n{oracle.tail(out, 400)}"

        if best is None or elapsed < best:
            best = elapsed

    return best, last_error


def check_suffix_scan(scale):
    """
    注释放末尾 vs 放开头：比值必须接近 1。

    退化时（每个 token 都 strnlen 到文件末尾）尾部注释会给每个 token
    加上"注释长度"的扫描量，比值随 token 数上升。
    """
    n = int(4000 * scale)
    body = gen_functions(n)
    comment = "// " + ("z" * BIG_COMMENT_BYTES) + "\n"

    head_src = write_case(f"suffix_head_{n}", comment + body)
    tail_src = write_case(f"suffix_tail_{n}", body + comment)

    t_head, err = time_compile(head_src)
    if err:
        return None, err, ""
    t_tail, err = time_compile(tail_src)
    if err:
        return None, err, ""

    ratio = t_tail / t_head if t_head > 0 else float("inf")
    detail = (f"head={t_head:.3f}s  tail={t_tail:.3f}s  ratio={ratio:.2f} "
              f"(limit {SUFFIX_RATIO_LIMIT})")
    return ratio <= SUFFIX_RATIO_LIMIT, detail, ratio


def check_linear_scaling(scale):
    """2N 个函数的耗时不应超过 N 的 3 倍（线性是 2 倍）。"""
    n = int(4000 * scale)

    t_small, err = time_compile(write_case(f"scale_{n}", gen_functions(n)))
    if err:
        return None, err, ""
    t_big, err = time_compile(write_case(f"scale_{n * 2}", gen_functions(n * 2)))
    if err:
        return None, err, ""

    ratio = t_big / t_small if t_small > 0 else float("inf")
    detail = (f"{n}={t_small:.3f}s  {n * 2}={t_big:.3f}s  ratio={ratio:.2f} "
              f"(limit {SCALING_RATIO_LIMIT})")
    return ratio <= SCALING_RATIO_LIMIT, detail, ratio


def main():
    quick = "--quick" in sys.argv
    scale = 0.5 if quick else 1

    if not os.path.exists(CREST):
        print(f"error: {CREST} not found in project root")
        return 1

    try:
        PERF_DIR.mkdir(parents=True, exist_ok=True)
        probe = PERF_DIR / ".write_probe"
        probe.write_text("x", encoding="utf-8")
        probe.unlink()
    except OSError as e:
        print(f"error: {PERF_DIR} is not writable ({e})")
        print("  换成可写目录重试: CREST_PERF_DIR=<dir> python test/perf.py")
        return 1

    print(f"=== Crest perf regression (CREST_PERF_DIR={PERF_DIR}) ===")
    print()

    checks = [("suffix_scan", check_suffix_scan), ("linear_scaling", check_linear_scaling)]

    failed = 0
    for name, fn in checks:
        print(f"  [case] {name}")
        ok, detail, _ = fn(scale)
        if ok is None:
            print(f"    [FAIL] {detail}")
            failed += 1
        elif ok:
            print(f"    [PASS] {detail}")
        else:
            print(f"    [FAIL] {detail}")
            failed += 1

    print()
    print(f"  {len(checks) - failed} passed, {failed} failed")

    if failed:
        print()
        print("  复杂度退化了：先确认 tokenizer 里构造 token 文本时没有对")
        print("  源码中段指针做 strlen（xp_make_string_capacity/count 曾有过）。")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

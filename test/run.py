"""
Crest compiler test runner.

Discovers direct children of test/pass/ and test/fail/, passes each
(file or directory) to `crest build`.  Pass/fail is determined by path.

Directives (in source as  // @name value  comments):
    // @skip              Skip this test (e.g. multi-file helper)
    // @run               Link .o → .exe with clang++, run it; non-zero exit → FAIL
    // @error "fragment"  For fail tests: compiler output must contain this
    // @link-c <file>     Compile <file> with cc and link it in (C ABI tests)
    // @static-lib        Build a .a, link @link-c driver against it, run it
    // @shared-lib        Build a .so/.dylib, link @link-c driver against it, run it

Fail tests are judged strictly: a compiler crash (assert/panic/segfault)
is never "the expected error".  Crash classification lives in oracle.py,
shared with fuzz.py.
"""

import subprocess
import pathlib
import sys
import re
import os
import shutil

import oracle

CREST = oracle.default_crest()
CLANG = "clang++"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RUN_DIR = os.path.join(SCRIPT_DIR, "build_test")


def clean_dir(path):
    """Remove all files in a directory."""
    if os.path.isdir(path):
        for f in os.listdir(path):
            filepath = os.path.join(path, f)
            for _ in range(3):
                try:
                    os.remove(filepath)
                    break
                except PermissionError:
                    import time
                    time.sleep(0.1)
    else:
        os.makedirs(path, exist_ok=True)


def parse_directives(text):
    """Extract // @key value directives from source text."""
    directives = {}
    for line in text.splitlines():
        m = re.match(r"\s*//\s*@([\w-]+)\s*(.*)", line)
        if not m:
            continue
        key, raw_val = m.group(1), m.group(2).strip()
        if key in ("pass", "fail", "run", "skip", "static-lib", "shared-lib"):
            directives[key] = True
        elif key in ("error", "link-c"):
            directives[key] = raw_val.strip('"').strip("'")
    return directives


def collect_directives(item):
    """Collect directives from all .cst files under `item` (file or dir)."""
    files = sorted(item.rglob("*.cst")) if item.is_dir() else [item]
    result = {}
    for f in files:
        d = parse_directives(f.read_text(encoding="utf-8"))
        if "link-c" in d:
            # 相对 .cst 所在目录解析，测试目录可整体移动
            d["link-c"] = str((f.parent / d["link-c"]).resolve())
        result.update(d)
    return result, files


def error_count_of(output):
    """Parse the compiler's own `N error(s), M warning(s) found` tally.

    Returns N, or None if the summary line is absent.  Matching this
    summary (rather than a bare `"error" in output`) is what stops a
    crash traceback that happens to contain the word from counting as a
    clean diagnostic.
    """
    m = re.search(r"^\s*(\d+)\s+error\(s\)", output, re.MULTILINE)
    return int(m.group(1)) if m else None


def lib_stem(item):
    """Library name stem: crest derives it from the main package path."""
    return item.stem if item.is_file() else item.name


def link_consumer(item, directives, lib_kind):
    """Compile the @link-c driver and link it against the built library.

    Returns (exe_path, error_message); exactly one is None.
    """
    link_c = directives.get("link-c")
    if not link_c:
        return None, f"@{lib_kind} needs @link-c (a C driver with main)"

    c_obj = os.path.join(RUN_DIR, pathlib.Path(link_c).stem + ".o")
    comp = subprocess.run(
        [C_COMPILER, "-c", link_c, "-o", c_obj],
        capture_output=True, text=True,
    )
    if comp.returncode != 0:
        return None, f"C compile error ({link_c}):\n{comp.stderr[:300]}"

    exe_path = os.path.join(RUN_DIR, "test.exe")
    link = subprocess.run(
        [C_COMPILER, c_obj, "-L", RUN_DIR, "-l" + lib_stem(item), "-o", exe_path],
        capture_output=True, text=True,
    )
    if link.returncode != 0:
        return None, f"link against the library failed:\n{link.stderr[:300]}"

    return exe_path, None


def library_run_env(lib_kind):
    """Shared libraries are located at runtime via the loader search path."""
    env = dict(os.environ)
    if lib_kind != "shared-lib":
        return env

    if sys.platform == "darwin":
        key = "DYLD_LIBRARY_PATH"
    elif os.name == "nt":
        key = "PATH"
    else:
        key = "LD_LIBRARY_PATH"

    env[key] = RUN_DIR + os.pathsep + env.get(key, "")
    return env


def run_and_judge(exe_path, env=None):
    """Run the linked executable; returns (returncode, failure_message)."""
    run_result = subprocess.run(
        [exe_path], capture_output=True, text=True, env=env,
    )
    if run_result.returncode == 0:
        return 0, None

    rverdict, rreason = oracle.judge(
        run_result.returncode,
        (run_result.stdout or "") + (run_result.stderr or ""))

    msg = f"runtime exit code {run_result.returncode}"
    if rverdict != oracle.OK:
        msg += f" [{rreason}]"
    msg += f"\n  stdout: {run_result.stdout[:300]}"
    msg += f"\n  stderr: {run_result.stderr[:300]}"
    return run_result.returncode, msg


def main():
    if not os.path.exists(CREST):
        print(f"error: {CREST} not found in project root")
        sys.exit(1)

    if shutil.which(CLANG) is None:
        print(f"error: {CLANG} not found in PATH")
        sys.exit(1)

    passed = 0
    failed = 0
    failures = []

    for category in ("pass", "fail"):
        cat_dir = pathlib.Path("test") / category
        if not cat_dir.exists():
            continue

        for item in sorted(cat_dir.iterdir()):
            directives, cst_files = collect_directives(item)

            if directives.get("skip"):
                print(f"  [SKIP] {item}")
                continue

            if directives.get("link-c") and C_COMPILER is None:
                print(f"  [SKIP] {item}  (no C compiler for @link-c)")
                continue

            lib_kind = None
            if directives.get("static-lib"):
                lib_kind = "static-lib"
            elif directives.get("shared-lib"):
                lib_kind = "shared-lib"

            # 库测试要用 C 驱动链接成品库：MSVC 目标的 .lib 名字和 GNU -l 规则对不上
            if lib_kind and os.name == "nt":
                print(f"  [SKIP] {item}  (library tests need a GNU target)")
                continue

            is_fail = (category == "fail")

            build_extra = []
            if lib_kind == "static-lib":
                build_extra.append("-static-lib")
            elif lib_kind == "shared-lib":
                build_extra.append("-shared")

            # Always output to test/build_test/ to avoid polluting project root
            clean_dir(RUN_DIR)
            rc, combined, timed_out = oracle.run_compiler(
                CREST, oracle.build_args(item, RUN_DIR, build_extra), timeout=60)

            verdict, reason = oracle.judge(rc, combined, timed_out)

            if verdict != oracle.OK:
                print(f"  [FAIL] {item}  (compiler {verdict.lower()})")
                failed += 1
                failures.append((str(item),
                    f"compiler {verdict.lower()}: {reason}\n"
                    f"  output: {oracle.tail(combined, 400)}"))
                clean_dir(RUN_DIR)
                continue

            n_errors = error_count_of(combined)

            if is_fail:
                if n_errors is None:
                    print(f"  [FAIL] {item}  (expected error, got none)")
                    failed += 1
                    failures.append((str(item),
                        "expected a compilation error but the compiler "
                        "printed no error summary"))
                elif n_errors == 0:
                    print(f"  [FAIL] {item}  (expected error, got none)")
                    failed += 1
                    failures.append((str(item),
                        f"compiler reported 0 errors\n"
                        f"  output: {oracle.tail(combined, 400)}"))
                elif "error" in directives and directives["error"] not in combined:
                    print(f"  [FAIL] {item}  (wrong error)")
                    failed += 1
                    failures.append((str(item),
                        f"expected error '{directives['error']}' not found\n"
                        f"  output: {oracle.tail(combined, 400)}"))
                else:
                    print(f"  [PASS] {item}  (error as expected)")
                    passed += 1
            else:
                if n_errors:
                    print(f"  [FAIL] {item}")
                    failed += 1
                    failures.append((str(item),
                        f"compilation had {n_errors} error(s):\n"
                        f"{oracle.tail(combined, 300)}"))
                elif lib_kind:
                    exe_path, err_msg = link_consumer(item, directives, lib_kind)
                    if err_msg:
                        print(f"  [FAIL] {item}  ({lib_kind})")
                        failed += 1
                        failures.append((str(item), err_msg))
                        clean_dir(RUN_DIR)
                        continue

                    rc_run, msg = run_and_judge(exe_path, library_run_env(lib_kind))
                    if rc_run != 0:
                        print(f"  [FAIL] {item}  (exited {rc_run})")
                        failed += 1
                        failures.append((str(item), msg))
                    else:
                        print(f"  [PASS] {item}")
                        passed += 1

                    clean_dir(RUN_DIR)
                elif directives.get("run"):
                    exe_path = os.path.join(RUN_DIR, "test.exe")

                    link_c = directives.get("link-c")
                    if link_c:
                        c_obj = os.path.join(RUN_DIR, pathlib.Path(link_c).stem + ".o")
                        comp = subprocess.run(
                            [C_COMPILER, "-c", link_c, "-o", c_obj],
                            capture_output=True, text=True,
                        )
                        if comp.returncode != 0:
                            print(f"  [FAIL] {item}  (C compile failed)")
                            failed += 1
                            failures.append((str(item),
                                f"C compile error ({link_c}):\n{comp.stderr[:300]}"))
                            clean_dir(RUN_DIR)
                            continue

                    obj_files = [os.path.join(RUN_DIR, f)
                                 for f in os.listdir(RUN_DIR) if f.endswith(".o")]
                    if not obj_files:
                        print(f"  [FAIL] {item}  (no .o files)")
                        failed += 1
                        failures.append((str(item), f"no .o files in {RUN_DIR}"))
                        continue

                    link = subprocess.run(
                        [CLANG] + obj_files + ["-o", exe_path],
                        capture_output=True, text=True,
                    )
                    if link.returncode != 0:
                        print(f"  [FAIL] {item}  (link failed)")
                        failed += 1
                        failures.append((str(item), f"link error:\n{link.stderr[:300]}"))
                        clean_dir(RUN_DIR)
                        continue

                    rc_run, msg = run_and_judge(exe_path)
                    if rc_run != 0:
                        print(f"  [FAIL] {item}  (exited {rc_run})")
                        failed += 1
                        failures.append((str(item), msg))
                    else:
                        print(f"  [PASS] {item}")
                        passed += 1

                    clean_dir(RUN_DIR)
                else:
                    print(f"  [PASS] {item}")
                    passed += 1

    print()
    print(f"  {passed} passed, {failed} failed")

    if failures:
        print()
        print("--- failures ---")
        for name, msg in failures:
            print(f"\n  [{name}]")
            for line in msg.splitlines():
                print(f"    {line}")

    clean_dir(RUN_DIR)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

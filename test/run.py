"""
Crest compiler test runner.

Discovers direct children of test/pass/ and test/fail/, passes each
(file or directory) to `crest build`.  Pass/fail is determined by path.

Directives (in source as  // @name value  comments):
    // @skip              Skip this test (e.g. multi-file helper)
    // @run               Link .o → .exe with clang++, run it; non-zero exit → FAIL
    // @error "fragment"  For fail tests: compiler output must contain this

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
        m = re.match(r"\s*//\s*@(\w+)\s*(.*)", line)
        if not m:
            continue
        key, raw_val = m.group(1), m.group(2).strip()
        if key in ("pass", "fail", "run", "skip"):
            directives[key] = True
        elif key == "error":
            directives[key] = raw_val.strip('"').strip("'")
    return directives


def collect_directives(item):
    """Collect directives from all .cst files under `item` (file or dir)."""
    files = sorted(item.rglob("*.cst")) if item.is_dir() else [item]
    result = {}
    for f in files:
        result.update(parse_directives(f.read_text(encoding="utf-8")))
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

            is_fail = (category == "fail")

            # Always output to test/build_test/ to avoid polluting project root
            clean_dir(RUN_DIR)
            rc, combined, timed_out = oracle.run_compiler(
                CREST, oracle.build_args(item, RUN_DIR), timeout=60)

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
                elif directives.get("run"):
                    exe_path = os.path.join(RUN_DIR, "test.exe")

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

                    run_result = subprocess.run(
                        [exe_path], capture_output=True, text=True,
                    )
                    rverdict, rreason = oracle.judge(
                        run_result.returncode,
                        (run_result.stdout or "") + (run_result.stderr or ""))
                    if run_result.returncode != 0:
                        print(f"  [FAIL] {item}  (exited {run_result.returncode})")
                        failed += 1
                        failures.append((str(item),
                            f"runtime exit code {run_result.returncode}"
                            + (f" [{rreason}]" if rverdict != oracle.OK else "")
                            + f"\n  stdout: {run_result.stdout[:300]}"
                            + f"\n  stderr: {run_result.stderr[:300]}"))
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

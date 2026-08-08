"""
Crest compiler test runner.

Discovers direct children of test/pass/ and test/fail/, passes each
(file or directory) to `crest build`.  Pass/fail is determined by path.

Directives (in source as  // @name value  comments):
    // @skip              Skip this test (e.g. multi-file helper)
    // @run               Link .o → .exe with clang++, run it; non-zero exit → FAIL
    // @error "fragment"  For fail tests: compiler output must contain this
"""

import subprocess
import pathlib
import sys
import re
import os
import shutil

CREST = "./crest.exe" if os.name == "nt" else "./crest"
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


def main():
    if not os.path.exists(CREST):
        print(f"error: {CREST} not found in project root")
        sys.exit(1)

    # check clang++ exists
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
            crest_args = [CREST, "build", str(item)]
            clean_dir(RUN_DIR)
            crest_args.extend(["-o", RUN_DIR])

            result = subprocess.run(
                crest_args,
                capture_output=True, text=True,
            )
            combined = result.stdout + result.stderr
            has_error = result.returncode != 0 or "error(s)" in combined

            if is_fail:
                if not has_error:
                    print(f"  [FAIL] {item}  (expected error, got none)")
                    failed += 1
                    failures.append((str(item), "expected compilation error but none reported"))
                elif "error" in directives and directives["error"] not in combined:
                    msg = f"expected error '{directives['error']}' not found in output\n  output: {combined[:400]}"
                    print(f"  [FAIL] {item}  (wrong error)")
                    failed += 1
                    failures.append((str(item), msg))
                else:
                    print(f"  [PASS] {item}  (error as expected)")
                    passed += 1
            else:
                if has_error:
                    tail = combined[-300:]
                    print(f"  [FAIL] {item}")
                    failed += 1
                    failures.append((str(item), f"compilation had errors:\n{tail}"))
                elif directives.get("run"):
                    exe_path = os.path.join(RUN_DIR, "test.exe")

                    # link all .o files in RUN_DIR
                    obj_files = [os.path.join(RUN_DIR, f) for f in os.listdir(RUN_DIR) if f.endswith(".o")]
                    if not obj_files:
                        print(f"  [FAIL] {item}  (no .o files in {RUN_DIR})")
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
                        [exe_path],
                        capture_output=True, text=True,
                    )
                    if run_result.returncode != 0:
                        print(f"  [FAIL] {item}  (exited {run_result.returncode})")
                        failed += 1
                        failures.append((str(item),
                            f"runtime exit code {run_result.returncode}\n"
                            f"  stdout: {run_result.stdout[:300]}\n"
                            f"  stderr: {run_result.stderr[:300]}"))
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

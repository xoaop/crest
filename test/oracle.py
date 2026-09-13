"""
Crest compiler test oracle — shared crash/verdict logic.

Used by both run.py (correctness suite) and fuzz.py (robustness suite).
The single invariant this module encodes:

    For ANY input, the compiler may only
      (a) succeed, or
      (b) report a clean, human-readable error and exit normally.
    There is no third outcome.

Any abnormal termination (assertion, panic, access violation, stack
overflow, heap corruption, ...) is a compiler bug, regardless of whether
the input was valid.

Why NTSTATUS matters on Windows:
    Windows reports crashes as NTSTATUS codes (e.g. 0xC00000FD =
    STATUS_STACK_OVERFLOW).  Python surfaces them as a large *positive*
    int (3221225725), NOT as a negative one — `returncode < 0` is always
    False on Windows.  On POSIX the same crash arrives as -signal.  Both
    are normalised here.
"""

import os
import re
import subprocess

# ---------------------------------------------------------------- exit codes

# Windows NTSTATUS crash codes.  Masked to 32 bits before comparison,
# because Python reports them as large positive ints.
CRASH_CODES = {
    0xC0000005: "STATUS_ACCESS_VIOLATION",
    0xC00000FD: "STATUS_STACK_OVERFLOW",
    0xC0000374: "STATUS_HEAP_CORRUPTION",
    0xC0000409: "STATUS_STACK_BUFFER_OVERRUN",
    0xC0000017: "STATUS_NO_MEMORY",
    0xC000013A: "STATUS_CONTROL_C_EXIT",
}

# POSIX signals that mean "the process died abnormally".
CRASH_SIGNALS = {
    4:  "SIGILL",
    6:  "SIGABRT",
    8:  "SIGFPE",
    10: "SIGBUS",
    11: "SIGSEGV",
}

# The compiler's own abort path (src/print.hpp) writes to stderr before
# calling std::abort():
#
#     println_err("[{}] {}:{}: {}", tag, loc.file_name(), loc.line(), ...)
#
# so a real abort looks like `[ASSERT] F:/Crest/src/print.hpp:78: ...`.
#
# This must be matched *structurally*, not by bare substring: the fuzzer
# feeds arbitrary text to the compiler, and the compiler echoes offending
# source lines back in diagnostics.  A generated file containing the word
# "PANIC" would otherwise be reported as a crash.  Requiring the bracket
# tag plus a .cpp/.hpp path plus a line number makes a collision
# effectively impossible.
ABORT_LINE_RE = re.compile(
    r"^\[(?:PANIC|ASSERT|ASSERT_MSG)\]"   # print.hpp tag
    r".*\.(?:cpp|hpp):\d+:"               # source_location
)

# Verdicts
OK = "OK"
CRASH = "CRASH"
HANG = "HANG"


def crash_reason(returncode, combined_output, timed_out=False):
    """Return a human-readable crash reason, or None if the run was clean.

    `returncode` may be None (process never started) — not a crash.
    `combined_output` is stdout+stderr concatenated.
    """
    if timed_out:
        return "timeout"

    if returncode is not None:
        code = returncode & 0xFFFFFFFF

        if code in CRASH_CODES:
            return f"{CRASH_CODES[code]} (0x{code:08X})"

        # POSIX: negative returncode == killed by signal
        if returncode < 0:
            sig = -returncode
            name = CRASH_SIGNALS.get(sig, "signal")
            return f"killed by {name} ({sig})"

    for line in combined_output.splitlines():
        if ABORT_LINE_RE.match(line.strip()):
            return f"compiler abort: {line.strip()[:120]}"

    return None


def judge(returncode, combined_output, timed_out=False):
    """Classify a completed run.  Returns (verdict, detail)."""
    reason = crash_reason(returncode, combined_output, timed_out)
    if reason is None:
        return OK, ""
    verdict = HANG if reason == "timeout" else CRASH
    return verdict, reason


def tail(text, n=400):
    """Last n chars, for error messages — the interesting part is at the end."""
    return text if len(text) <= n else "..." + text[-n:]


# ------------------------------------------------------------ running crest

def default_crest():
    return "./crest.exe" if os.name == "nt" else "./crest"


def run_compiler(crest, args, timeout=10, cwd=None, extra_env=None):
    """Run the compiler, never raising.

    Returns (returncode, combined_output, timed_out).
    A timeout kills the process and reports timed_out=True; a failure to
    launch is reported as returncode=None with the OSError text.
    """
    env = None
    if extra_env:
        env = dict(os.environ)
        env.update(extra_env)

    try:
        p = subprocess.run(
            [crest] + list(args),
            capture_output=True, text=True, errors="replace",
            timeout=timeout, cwd=cwd, env=env,
        )
    except subprocess.TimeoutExpired as e:
        # Partial output still tells us where it was when it hung.
        out = ""
        for stream in (e.stdout, e.stderr):
            if stream:
                out += stream if isinstance(stream, str) else stream.decode("utf-8", "replace")
        return None, out, True
    except OSError as e:
        return None, f"<failed to launch: {e}>", False

    return p.returncode, (p.stdout or "") + (p.stderr or ""), False


def build_args(src, out_dir):
    """Canonical `crest build` argv for a source path or directory."""
    return ["build", str(src), "-o", str(out_dir)]

@echo off
rem Crest compiler robustness fuzzer.
rem
rem Unlike test.bat (which checks that valid programs compile and invalid
rem ones report the RIGHT error), this only checks one invariant:
rem
rem     the compiler must never crash -- for any input.
rem
rem A crash is an assertion, a panic, an access violation, a stack
rem overflow, or a hang.  See test/oracle.py for the classification.
rem
rem The first run WILL be red: deep nesting currently overflows the stack.
rem That is the point -- it is a real, reproducible defect.
setlocal
cd /d "%~dp0"

python test\fuzz.py %*
echo.
echo Exit code: %ERRORLEVEL%

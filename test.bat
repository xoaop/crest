@echo off
echo === Crest Test Suite ===
echo.
python test/run.py
echo.
echo Exit code: %ERRORLEVEL%

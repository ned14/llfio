@ECHO OFF

ECHO. 2> "%~dp0test_cpps.txt"

FOR %%G IN ("%~dp0\tests\*_test.cpp") DO ECHO #include "tests/%%~nxG"  >> "%~dp0test_cpps.txt"


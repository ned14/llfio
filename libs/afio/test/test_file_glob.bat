@ECHO OFF

ECHO. 2> "%~dp0test_cpps.txt"

FOR %%G IN ("%~dp0\tests\*.cpp") DO ECHO #include "tests/%%~nxG"  >> "%~dp0test_cpps.txt"


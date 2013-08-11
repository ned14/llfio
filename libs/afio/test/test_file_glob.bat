@ECHO OFF

ECHO. 2> "%~dp0test_cpps.txt"

FOR %%G IN ("%~dp0*_test.cpp") DO ECHO #include "%%~nxG"  >> "%~dp0test_cpps.txt"


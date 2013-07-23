@ECHO OFF


ECHO. 2> test_cpps.txt

FOR %%G IN (*_test.cpp) DO ECHO #include "%%G"  >> test_cpps.txt


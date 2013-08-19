#!/bin/sh

> ${0%/*}/test_cpps.txt

for filename in ${0%/*}/tests/*_test.cpp
do
    echo "#include \"tests/${filename##${0%/*}/}\""  >> ${0%/*}/test_cpps.txt
done

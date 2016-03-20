#!/bin/sh

> ${0%/*}/test_cpps.txt

for filename in ${0%/*}/tests/*.cpp
do
    echo "#include \"${filename##${0%/*}/}\""  >> ${0%/*}/test_cpps.txt
done

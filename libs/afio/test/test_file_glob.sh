#! /bin/sh


> test_cpps.txt

for filename in *_test.cpp
do
    echo "#include \"$filename\""  >> test_cpps.txt
done

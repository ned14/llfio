#!/bin/bash
rm -rf send_to_wandbox_tmp
mkdir send_to_wandbox_tmp
include/boost/afio/bindlib/scripts/GenSingleHeader.py -DBOOST_AFIO_USE_BOOST_FILESYSTEM=1 -DBOOST_AFIO_DISABLE_VALGRIND=1 -Eafio_iocp.ipp -Ent_kernel_stuff -Evalgrind -Amonad_policy -Afuture_policy include/boost/afio/afio.hpp > include/boost/afio/single_include.hpp
sed "1s/.*/#include \"afio_single_include.hpp\"/" example/readwrite_example.cpp > send_to_wandbox.cpp
#cd send_to_wandbox_tmp
#sed "s/#include/@include/g" ../include/boost/afio/single_include.hpp > afio_single_include.hpp
#g++ -std=c++11 -E afio_single_include.hpp > afio_single_include2.hpp
#sed "s/@include/#include/g" afio_single_include2.hpp > afio_single_include.hpp
#sed "s/# [0-9][0-9]* \".*\".*//g" afio_single_include.hpp > afio_single_include2.hpp
#sed "/^$/d" afio_single_include2.hpp > afio_single_include.hpp
#rm afio_single_include2.hpp
#cd ..
gcc -fpreprocessed -dD -E -P include/boost/afio/single_include.hpp > send_to_wandbox_tmp/afio_single_include2.hpp 2>/dev/null
sed "/^$/d" send_to_wandbox_tmp/afio_single_include2.hpp > send_to_wandbox_tmp/afio_single_include.hpp
rm -rf send_to_wandbox_tmp/afio_single_include2.hpp
#include/boost/afio/bindlib/scripts/send_to_wandbox.py send_to_wandbox_tmp send_to_wandbox.cpp
URL=`include/boost/afio/bindlib/scripts/send_to_wandbox.py send_to_wandbox_tmp send_to_wandbox.cpp | sed -e 's/.*\(http:\/\/[^ '"'"']*\).*/\1/'`
if [[ $FRAME != "" ]]; then
    echo '<iframe src="'$URL'" frameborder="0" style="height: 100%; width: 100%;" height="100%" width="100%"></iframe>'
else
    echo $URL
fi
rm -rf send_to_wandbox_tmp

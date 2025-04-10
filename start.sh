#!/bin/sh

if [ "$1" = "c" ] ; then  # (re)compile everything
    mkdir build
    cd build
	rm -rf CMakeFiles
	rm CMakeCache.txt
	cmake -D CMAKE_BUILD_TYPE=Debug ..
	make
    exit
elif [ "$1" = "format" ]; then # run clang-format
    find "$(dirname $0)/src" -iname '*.h' -o -iname '*.cpp' | xargs clang-format --style=file -i
    exit
fi

set -e
cd ./build
make
set +e
cd ..
rm callgrind.* > /dev/null 2>&1 # rm callgrind.* &> /dev/null
echo "-------------------------------------------------"
set -e


if [ "$1" = "perf" ] ; then  # perf test
    shift 1
    valgrind --tool=callgrind --collect-systime=usec ./build/game_engine "$@"
    qcachegrind
elif [ "$1" = "mem" ] ; then # memory test
    shift 1
    valgrind ./build/game_engine "$@"
else
	./build/game_engine "$@"
fi

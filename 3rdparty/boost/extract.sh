#!/bin/bash

#
# Script to extract only needed boost files using the bcp tool:
#
# http://www.boost.org/doc/libs/1_47_0/tools/bcp/doc/html/index.html
#
# Does also work with an outdated bcp version
#
# Usage: extract.sh <path to new boost version>
#

HEADERS="\
  boost/assert.hpp \
  boost/crc.hpp \
  "

if [ -z $1 ]
then
    echo "Usage: extract.sh [--report] <path to new boost version>"
    echo "Update our local boost copy"
    echo "With --report, create a HTML report that contains in particular the dependencies"
    exit 1
fi

if [ x$1 = x--report ]; then
    bcp --boost=$2 --report $HEADERS report.html
    exit 0;
fi


rm -rf needed
mkdir needed

bcp --boost=$1 $HEADERS needed/

# we do not use the provided MSVC project files
find needed -name '*.vcpro*' | xargs rm

# remove old boost code
rm -rf boost/*

# copy new headers
cp -vR needed/boost .

rm -rf needed



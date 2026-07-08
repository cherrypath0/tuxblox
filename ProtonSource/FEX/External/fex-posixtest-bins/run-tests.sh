#!/bin/bash

echo
echo "Running fexint ($1)"
rm -rf fexint
time ./test-runner.sh "$1 -U -c irint -n 500 -R $ROOTFS -- " fexint

echo
echo "Running fexjit ($1)"
rm -rf fexjit
time ./test-runner.sh "$1 -U -c irjit -n 500 -R $ROOTFS -- " fexjit

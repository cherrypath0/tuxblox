#!/bin/bash

echo "Running native"
rm -rf native
time ./test-runner.sh ./ native

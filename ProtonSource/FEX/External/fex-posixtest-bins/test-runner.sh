#!/bin/bash

mkdir -p $2

shopt -s globstar
echo log start > $2/results.out
for i in **/*.test; do # Whitespace-safe and recursive
    echo running $i
    timeout 45s $1"$i" &>$2/${i//\//-}.out
    echo ${i//\//-} $? >> $2/results.out
done

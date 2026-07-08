#!/usr/bin/env python3

native_log = open("native/results.out","r")
fexint_log = open("fexint/results.out","r")
fexjit_log = open("fexjit/results.out","r")

fexint_fails = 0
fexjit_fails = 0
total_tests = 0

while True:
    native_line = native_log.readline().strip("\n")

    if not native_line:
        break

    total_tests = total_tests + 1
    fexint_line = fexint_log.readline().strip("\n")
    fexjit_line = fexjit_log.readline().strip("\n")

    if native_line != fexint_line:
        print("Fexint failure:", native_line, "vs", fexint_line)
        fexint_fails = fexint_fails + 1;
    if native_line != fexjit_line:
        print("Fexjit failure:", native_line, "vs", fexjit_line)
        fexjit_fails = fexjit_fails + 1;

print("fexint:", fexint_fails, "failed", total_tests - fexint_fails, "passed")
print("fexjit:", fexjit_fails, "failed", total_tests - fexjit_fails, "passed")
print(total_tests, "total tests")

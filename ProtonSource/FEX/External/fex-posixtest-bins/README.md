Bins from posixtest 1.5.2. Simply built using make.

Simplistic test runner script that can be used as
```
./run-tests.sh "./" native.log # run native
./run-tests.sh "/home/skmp/projects/FEX/build/Bin/ELFLoader -U -c irint -n 500 -- " fexint.log # run fex interpreter
./run-tests.sh "/home/skmp/projects/FEX/build/Bin/ELFLoader -U -c irjit -n 500 -- " fexjit.log # run fex jit
```

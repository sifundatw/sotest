# sotest

A scripting language interpreter for testing shared function libraries.
C99, console application for GNU/Linux (x86), POSIX.1-2001 (uses `dlopen`/`dlsym`).

## Build

Requires `gcc` and `make`.

```sh
make            # builds sotest, test.so and other.so
make check      # runs ./sotest test.sc
make clean
```

Manual build:

```sh
gcc -std=c99 -Wall -Wextra -pedantic -O2 -o sotest sotest.c -ldl
gcc -std=c99 -fPIC -shared -o test.so libs/test.c
gcc -std=c99 -fPIC -shared -o other.so libs/other.c
```

## Usage

```sh
./sotest test.sc      # run a script
./sotest              # interactive mode
```

## Language

One line is one command:

| Command | Meaning |
|---|---|
| `use <so_name>` | load a shared library (*.so) |
| `call <func_name>` | call `void func(void)` from the loaded libraries (latest loaded first) |

Extras: comments (`#`, `;`) and line continuation (`\`).

- Syntax errors (unknown command, missing/extra arguments): warning, script continues.
- Execution errors (file not found, not a shared library, function not found, no library loaded): error message, script stops with exit status 1.

## Self-test

`make check` output:

```
[test.so] example() called
[test.so] test1_func() called
[other.so] func1() called
warning: test.sc:8: unknown command 'bogus' (ignored)
warning: test.sc:9: 'call' requires an argument (ignored)
warning: test.sc:10: extra arguments after 'use other.so' (ignored)
[test.so] example() called
error: function 'missing_func' not found in loaded libraries
       at test.sc:13
exit status: 1
```

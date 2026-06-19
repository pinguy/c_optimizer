# Toolchains

Tiny C releases are sensitive to compiler versions. Two compilers can produce a very similar raw ELF size but a different final runner size because x86 BCJ and LZMA respond to instruction layout and byte patterns.

## GCC 9 Bullseye Wrapper

The repo includes a Podman-backed Debian Bullseye GCC 9 wrapper:

```bash
./build_gcc9_bullseye.sh nervk.c
```

That writes `./nervk`. From this checkout, use `examples/nervk.c`, `examples/VOIDRUNNER.c`, or `examples/ECHOHULL.c`; generated example runners are ignored by git.

On first use, the wrapper builds this local image:

```text
localhost/c-optimizer-gcc9:bullseye
```

The image is built from `toolchains/gcc9-bullseye/Containerfile`. It installs Debian `gcc-9`, binutils, xz, Python, and libc headers, then points `/usr/local/bin/gcc` at `gcc-9`.

To force a rebuild after changing the Containerfile:

```bash
COPT_GCC9_REBUILD=1 ./build_gcc9_bullseye.sh examples/nervk.c
```

The wrapper mounts:

- the C Optimizer checkout read-only at `/c_optimizer`
- the selected source directory read-only at `/input`
- the output directory read-write at `/output`

It then runs:

```text
CC=gcc COPT_OPT=-Os COPT_EXTRA_LDLIBS=-ldl ./build_asm_syscall.sh ...
```

`-Os` is used because GCC 9 predates GCC's `-Oz` support. `-ldl` is needed because Bullseye's older glibc keeps `dlopen`/`dlsym` in `libdl.so.2`.

Set `OUT` only when you want to override the default beside-source output path:

```bash
OUT=release/nervk ./build_gcc9_bullseye.sh nervk.c
```

## Why GCC 9?

For tiny packed C releases, newer is not automatically smaller. The final runner is a shell stub plus an x86-BCJ-transformed LZMA payload, so instruction layout and repeated byte patterns matter as much as the raw ELF size.

On the current reference machine, GCC 9 was the smallest tested toolchain for VOIDRUNNER and also made nervk smaller:

```text
Source      Host GCC 16 runner   GCC 9 Bullseye runner   Saved
nervk       31,841 bytes         31,472 bytes            369 bytes / 1.16%
VOIDRUNNER  33,934 bytes         33,543 bytes            391 bytes / 1.15%
```

The wrapper exists so that release-size checks can use the known-good Debian Bullseye GCC 9/binutils/xz combination without changing the host compiler.

## Why Not Commit GCC Binaries?

The project carries a container recipe and wrapper instead of raw GCC/binutils/glibc binaries because a vendored toolchain is large, distro-coupled, and harder to keep legally and operationally tidy. The local image gives repeatable builds without putting hundreds of megabytes of compiler runtime into git.

## Tested VOIDRUNNER Compiler Sizes

These were measured with `examples/VOIDRUNNER.c`. Results are not byte-for-byte promises for every host or source file.

```text
Toolchain              Runner size   Difference vs local GCC 16
GCC 16 local           33,934 bytes  baseline
GCC 15 local           33,876 bytes  58 bytes smaller / 0.17%
GCC 14 trixie          33,730 bytes  204 bytes smaller / 0.60%
GCC 13 trixie          33,614 bytes  320 bytes smaller / 0.94%
GCC 12 trixie          33,572 bytes  362 bytes smaller / 1.07%
GCC 11 bookworm        33,564 bytes  370 bytes smaller / 1.09%  (-Oz -> -Os)
GCC 10 bullseye        33,562 bytes  372 bytes smaller / 1.10%  (-Os + -ldl)
GCC 9  bullseye        33,543 bytes  391 bytes smaller / 1.15%  (-Os + -ldl)
GCC 8  buster          33,842 bytes  92 bytes smaller / 0.27%   (-Os + -ldl)
GCC 7  buster          33,972 bytes  38 bytes larger / 0.11%    (-Os + -ldl)
GCC 6  stretch         34,486 bytes  552 bytes larger / 1.63%   (-Os + -ldl, minus unsupported -fno-code-hoisting)
```

GCC 9 was the smallest tested toolchain for VOIDRUNNER. The trend reversed after GCC 9.

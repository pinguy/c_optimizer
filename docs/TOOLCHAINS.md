# Toolchains

Tiny C releases are sensitive to compiler versions. Two compilers can produce a very similar raw ELF size but a different final runner size because x86 BCJ and LZMA respond to instruction layout and byte patterns.

## GCC 9 Bullseye Wrapper

The repo includes a Podman-backed Debian Bullseye GCC 9 wrapper:

```bash
OUT=/tmp/VOIDRUNNER-gcc9 ./build_gcc9_bullseye.sh examples/VOIDRUNNER.c
```

On first use, the wrapper builds this local image:

```text
localhost/c-optimizer-gcc9:bullseye
```

The image is built from `toolchains/gcc9-bullseye/Containerfile`. It installs Debian `gcc-9`, binutils, xz, Python, and libc headers, then points `/usr/local/bin/gcc` at `gcc-9`.

To force a rebuild after changing the Containerfile:

```bash
COPT_GCC9_REBUILD=1 ./build_gcc9_bullseye.sh examples/VOIDRUNNER.c
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

## Why Not Commit GCC Binaries?

The project carries a container recipe and wrapper instead of raw GCC/binutils/glibc binaries because a vendored toolchain is large, distro-coupled, and harder to keep legally and operationally tidy. The local image gives repeatable builds without putting hundreds of megabytes of compiler runtime into git.

## Tested VOIDRUNNER Sizes

These were measured with `examples/VOIDRUNNER.c`. Results are not byte-for-byte promises for every host or source file.

```text
GCC 16 local:          33,934 bytes
GCC 15 local:          33,876 bytes
GCC 14 trixie:         33,730 bytes
GCC 13 trixie:         33,614 bytes
GCC 12 trixie:         33,572 bytes
GCC 11 bookworm:       33,564 bytes  (-Oz -> -Os)
GCC 10 bullseye:       33,562 bytes  (-Os + -ldl)
GCC 9  bullseye:       33,543 bytes  (-Os + -ldl)
GCC 8  buster:         33,842 bytes  (-Os + -ldl)
GCC 7  buster:         33,972 bytes  (-Os + -ldl)
GCC 6  stretch:        34,486 bytes  (-Os + -ldl, minus unsupported -fno-code-hoisting)
```

GCC 9 was the smallest tested toolchain for VOIDRUNNER. The trend reversed after GCC 9.

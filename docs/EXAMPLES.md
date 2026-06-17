# Examples

The repository includes two example sources:

- `examples/VOIDRUNNER.c` - real single-file procedural space-trader / combat game source.
- `examples/nervk.c` - real single-file 96K-spirit procedural game source.

The real examples are intentionally large enough to show why C Optimizer exists: they generate assets at runtime, use a single C translation unit, and benefit from the custom `_start`, stripping, BCJ, LZMA, and shell-runner path.

## Quick Start

```bash
./build_gcc9_bullseye.sh examples/nervk.c
```

Expected shape:

```text
nervk:
  strip:    71.4 KiB (73,088 bytes, 584,704 bits)
  sstrip:   70.2 KiB (71,856 bytes, 574,848 bits)
  bcj/lzma: 30.6 KiB (31,325 bytes, 250,600 bits)
  runner:   30.7 KiB (31,472 bytes, 251,776 bits)
  NEEDED:   libdl.so.2
  NEEDED:   libc.so.6
```

Exact sizes vary by compiler/binutils/xz versions. The first GCC 9 wrapper run builds a local Podman image; later runs reuse it.

## Build The Real Examples

By default, generated runners are written beside the selected source file. The example outputs below are ignored by git:

```bash
./build_gcc9_bullseye.sh examples/nervk.c
./build_gcc9_bullseye.sh examples/VOIDRUNNER.c
```

On the current reference machine, the GCC 9 wrapper produced:

```text
nervk:
  strip:    71.4 KiB (73,088 bytes, 584,704 bits)
  sstrip:   70.2 KiB (71,856 bytes, 574,848 bits)
  bcj/lzma: 30.6 KiB (31,325 bytes, 250,600 bits)
  runner:   30.7 KiB (31,472 bytes, 251,776 bits)
  NEEDED:   libdl.so.2
  NEEDED:   libc.so.6

VOIDRUNNER:
  strip:    82.4 KiB (84,368 bytes, 674,944 bits)
  sstrip:   81.2 KiB (83,136 bytes, 665,088 bits)
  bcj/lzma: 32.6 KiB (33,396 bytes, 267,168 bits)
  runner:   32.8 KiB (33,543 bytes, 268,344 bits)
  NEEDED:   libdl.so.2
```

Treat those as examples, not guaranteed byte-for-byte promises.

## GCC 9 Size Difference

Tiny sizecoding builds are sensitive to compiler, binutils, and xz versions. The same source can produce the same or similar raw ELF size but compress differently after x86 BCJ and LZMA because the machine-code layout changed.

Compared with the host GCC 16 build on the current reference machine, the GCC 9 wrapper saved:

```text
Source      Host GCC 16 runner   GCC 9 Bullseye runner   Saved
nervk       31,841 bytes         31,472 bytes            369 bytes / 1.16%
VOIDRUNNER  33,934 bytes         33,543 bytes            391 bytes / 1.15%
```

Use `OUT` only when you want to override the default beside-source output path:

```bash
OUT=release/nervk ./build_gcc9_bullseye.sh examples/nervk.c
```

See [Toolchains](TOOLCHAINS.md) for more detail and compiler comparisons.

## Runtime Smoke

The packed files are interactive SDL/OpenGL programs. A useful runtime smoke needs a real display session or an Xvfb/llvmpipe-style headless GL setup.

If you have Xvfb available, try:

```bash
LIBGL_ALWAYS_SOFTWARE=1 timeout 10s xvfb-run -a examples/voidrunner --seed 1
LIBGL_ALWAYS_SOFTWARE=1 timeout 10s xvfb-run -a examples/nervk --seed 1
```

Without Xvfb, `SDL_VIDEODRIVER=dummy` is only a runner sanity check. These examples may reach display setup and then fail or wait because they need an OpenGL-capable video backend.

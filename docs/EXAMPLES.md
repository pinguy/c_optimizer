# Examples

The repository includes three example sources:

- `examples/hello.c` - minimal sanity check.
- `examples/VOIDRUNNER.c` - real single-file procedural space-trader / combat game source.
- `examples/nervk.c` - real single-file 96K-spirit procedural game source.

The real examples are intentionally large enough to show why C Optimizer exists: they generate assets at runtime, use a single C translation unit, and benefit from the custom `_start`, stripping, BCJ, LZMA, and shell-runner path.

## Minimal Example

```bash
./run_c_optimizer.sh examples/hello.c
./examples/hello
rm -f examples/hello
```

Expected shape:

```text
[build] runner:   505 B (505 bytes, 4,040 bits)
hello from c_optimizer
```

Exact size can vary by compiler/binutils/xz versions.

## Build The Real Examples

Use `OUT=...` to keep generated runners out of the repo:

```bash
rm -rf /tmp/copt-real-examples
mkdir -p /tmp/copt-real-examples

OUT=/tmp/copt-real-examples/VOIDRUNNER ./build_asm_syscall.sh examples/VOIDRUNNER.c
OUT=/tmp/copt-real-examples/nervk ./build_asm_syscall.sh examples/nervk.c
```

On the current reference machine, those build smoke tests produced:

```text
VOIDRUNNER:
  strip:    82.5 KiB (84,448 bytes, 675,584 bits)
  sstrip:   81.2 KiB (83,136 bytes, 665,088 bits)
  bcj/lzma: 33.0 KiB (33,787 bytes, 270,296 bits)
  runner:   33.1 KiB (33,934 bytes, 271,472 bits)
  NEEDED:   libc.so.6

nervk:
  strip:    70.1 KiB (71,824 bytes, 574,592 bits)
  sstrip:   68.9 KiB (70,512 bytes, 564,096 bits)
  bcj/lzma: 31.0 KiB (31,694 bytes, 253,552 bits)
  runner:   31.1 KiB (31,841 bytes, 254,728 bits)
  NEEDED:   libc.so.6
```

Treat those as examples, not guaranteed byte-for-byte promises.

## GCC 9 Bullseye Size Build

Tiny sizecoding builds are sensitive to compiler, binutils, and xz versions. The same source can produce the same or similar raw ELF size but compress differently after x86 BCJ and LZMA because the machine-code layout changed.

For `examples/VOIDRUNNER.c`, Debian Bullseye `gcc-9 (Debian 9.3.0-22) 9.3.0` produced the smallest tested runner so far:

```text
VOIDRUNNER with Debian GCC 9.3.0-22, binutils 2.35.2, xz 5.2.5:
  strip:    82.4 KiB (84,368 bytes, 674,944 bits)
  sstrip:   81.2 KiB (83,136 bytes, 665,088 bits)
  bcj/lzma: 32.6 KiB (33,396 bytes, 267,168 bits)
  runner:   32.8 KiB (33,543 bytes, 268,344 bits)
```

Use the bundled wrapper to reproduce that toolchain without changing the host system:

```bash
rm -rf /tmp/copt-gcc9-build
mkdir -p /tmp/copt-gcc9-build
OUT=/tmp/copt-gcc9-build/VOIDRUNNER ./build_gcc9_bullseye.sh examples/VOIDRUNNER.c
```

The first run builds a local `localhost/c-optimizer-gcc9:bullseye` image; later runs reuse it. See [Toolchains](TOOLCHAINS.md) for more detail and compiler comparisons.

## Runtime Smoke

The packed files are interactive SDL/OpenGL programs. A useful runtime smoke needs a real display session or an Xvfb/llvmpipe-style headless GL setup.

If you have Xvfb available, try:

```bash
LIBGL_ALWAYS_SOFTWARE=1 timeout 10s xvfb-run -a /tmp/copt-real-examples/VOIDRUNNER --seed 1
LIBGL_ALWAYS_SOFTWARE=1 timeout 10s xvfb-run -a /tmp/copt-real-examples/nervk --seed 1
```

Without Xvfb, `SDL_VIDEODRIVER=dummy` is only a runner sanity check. These examples may reach display setup and then fail or wait because they need an OpenGL-capable video backend.

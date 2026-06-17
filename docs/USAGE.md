# Usage

## Build From The Terminal

```bash
./build_gcc9_bullseye.sh nervk.c
```

The runnable is written beside the source file using a safe lowercase version of the source basename. For `nervk.c`, the output is `./nervk`.

The GCC 9 wrapper is the recommended release path when byte count matters. It builds inside a local Podman image and reuses that image after the first run.

For a host-compiler build, use:

```bash
./build_asm_syscall.sh nervk.c
```

For a GUI picker, use:

```bash
./run_c_optimizer.sh
```

Examples:

```text
nervk.c      -> nervk
VOIDRUNNER.c -> voidrunner
my demo.c    -> my-demo
```

To keep generated output somewhere else, set `OUT`:

```bash
OUT=release/nervk ./build_gcc9_bullseye.sh nervk.c
```

## Use The File Picker

Run:

```bash
./run_c_optimizer.sh
```

If `kdialog` or `zenity` is available, a graphical picker opens. Without a graphical picker, the script prompts for a path in the terminal.

## Install The Desktop Launcher

```bash
./install_desktop.sh
```

This creates:

```text
~/.local/share/applications/c-optimizer.desktop
```

The installed launcher points at the current checkout directory. If you move the checkout, run `./install_desktop.sh` again.

## Direct Builder

The lower-level builder is:

```bash
./build_asm_syscall.sh /path/to/file.c
```

Useful environment variables:

- `OUT=/path/to/output` overrides the output path.
- `WORKDIR=/path/to/workdir` keeps intermediates in a chosen directory.
- `RAW=/path/to/raw-elf` overrides the raw linked ELF path.
- `SST=/path/to/stripped-elf` overrides the section-stripped ELF path.
- `CC=/path/to/gcc` chooses the C compiler.
- `COPT_OPT=-Os` overrides the default optimization level (`-Oz`).
- `COPT_EXTRA_LDLIBS=-ldl` appends linker libraries after the normal linker flags.

Example:

```bash
./build_asm_syscall.sh nervk.c
```

## GCC 9 Bullseye Toolchain

For size-sensitive release builds, use the bundled Podman wrapper:

```bash
./build_gcc9_bullseye.sh nervk.c
```

From this checkout, the included nervk source is under `examples/`:

```bash
./build_gcc9_bullseye.sh examples/nervk.c
```

The first run builds a local `localhost/c-optimizer-gcc9:bullseye` image from `toolchains/gcc9-bullseye/Containerfile`. Later runs reuse that image. The wrapper sets the compatibility flags needed by Debian Bullseye's GCC 9 toolchain and writes the runner beside the source unless `OUT` is set.

See [Toolchains](TOOLCHAINS.md) for the tested compiler-size table.

## Source Layout Assumptions

C Optimizer adds these include paths:

```text
source file directory
repo/compat
```

That means a selected file can include local headers next to itself, and can also include the bundled minimal SDL compatibility headers:

```c
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
```

For tiny SDL/OpenGL productions, the smallest release source often uses `dlopen`/`dlsym` for heavier libraries and keeps only `libc` as a direct dependency. C Optimizer will show the raw ELF's `DT_NEEDED` list at the end of each build so you can see what the linker actually kept.

## What Gets Published

Generated runners are output beside selected source files. They are not intended to be committed unless you explicitly want to publish a built artifact.

The C Optimizer tool itself is Apache-2.0 licensed. Generated runners follow the license/ownership of the C source they were built from.

The checked-in real examples keep their own upstream/source licensing context; use them as build references and learning material.

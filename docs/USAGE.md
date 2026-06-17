# Usage

## Build From The Terminal

```bash
./run_c_optimizer.sh /absolute/or/relative/path/to/file.c
```

The runnable is written beside the source file using a safe lowercase version of the source basename.

Examples:

```text
hello.c      -> hello
VOIDRUNNER.c -> voidrunner
my demo.c    -> my-demo
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

Example:

```bash
OUT=/tmp/tiny-demo ./build_asm_syscall.sh examples/hello.c
```

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

## What Gets Published

Generated runners are output beside selected source files. They are not intended to be committed unless you explicitly want to publish a built artifact.

The C Optimizer tool itself is Apache-2.0 licensed. Generated runners follow the license/ownership of the C source they were built from.

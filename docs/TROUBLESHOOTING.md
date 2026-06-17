# Troubleshooting

## `expected a single C source file`

The selected file must end in `.c` or `.C`.

This tool intentionally does not build multi-file C projects.

## `source file not found`

Use an existing path, or run `./run_c_optimizer.sh` without arguments and pick the file graphically.

## Missing `gcc`, `xz`, `readelf`, or `python3`

Install the required toolchain packages for your distribution.

On Arch/CachyOS-like systems:

```bash
sudo pacman -S gcc xz binutils python
```

## The GUI Picker Does Not Open

Install one of:

- `kdialog`
- `zenity`

The script still works from a terminal without them.

## The Program Builds But Does Not Run

Check dynamic library dependencies:

```bash
readelf -d /path/to/raw-or-output
```

The build script prints `DT_NEEDED` for the raw ELF. If your C file uses SDL, OpenGL, math, or other libraries directly, you may need to load them dynamically from your program or adapt the linker flags.

For demoscene-style release builds, test the generated runner itself, not just the raw ELF. The runner also needs `xz` on the target system because it self-extracts from raw x86+LZMA data.

The included VOIDRUNNER/nervk examples are interactive SDL/OpenGL programs. They need a real display or headless GL setup. `SDL_VIDEODRIVER=dummy` is not a full runtime smoke for them.

## SDL Headers Are Minimal

The headers in `compat/SDL2/` are compatibility stubs for tiny single-file builds. They are not full SDL development headers.

If your source needs more SDL definitions, either add them to the compatibility headers or install/use the real SDL2 development headers and adjust the include/link approach.

## Output Is Larger Than Expected

That is allowed. The runner still works.

The builder reports the final runner using the best-fit IEC unit and exact byte/bit counts. Try reducing source size, assets, strings, static data, or library references if the output needs to be smaller.

Useful sizecoding checks:

- Look at `DT_NEEDED`; direct SDL/OpenGL/libm links can cost more than loading them dynamically.
- Remove unused compatibility header declarations from the source path when experimenting.
- Delete debug strings and large text blocks.
- Prefer deterministic procedural generation over embedded static data when it compresses better.
- Keep measuring the final runner; source size is only a hint.

## Temporary Files

By default, build intermediates live in a temporary directory and are removed after the build.

To inspect intermediates:

```bash
WORKDIR=/tmp/copt-debug ./build_asm_syscall.sh /path/to/file.c
```

# C Optimizer

C Optimizer turns a single C source file into a tiny runnable Linux executable wrapper.

It was built for demoscene / sizecoding projects like **VOIDRUNNER** and **nervk**: native Linux games or intros where the whole project lives in one `.c` file, assets are generated at runtime, and the release artefact is judged by bytes on disk.

## What It Does

- Opens a file picker for one `.c` file.
- Builds that file with size-focused GCC flags.
- Uses a tiny custom `_start` instead of normal startup files.
- Strips and section-strips the ELF.
- Searches several x86 BCJ + raw LZMA settings.
- Emits a self-extracting runnable beside the source file.
- Reports output sizes using the best-fit IEC unit plus exact bytes and bits.

The original `.c` file is left untouched.

## What It Is For

C Optimizer is aimed at projects with constraints like:

- one translation unit
- no external asset files
- procedural textures, geometry, maps, audio, or UI generated in code
- dynamic use of system libraries where useful
- Linux x86_64 release builds where the final runnable size matters
- fast iteration from a single C file to a releasable packed runner

Reference-style projects:

- [VOIDRUNNER](https://github.com/pinguy/VOIDRUNNER) - a native Linux procedural space-trader / combat game released as a 32 KiB-class runnable.
- [nervk / murkk](https://github.com/pinguy/murkk) - a 96K-spirit native Linux procedural game lineage with runtime-synthesized visuals, level, audio, and font data.

The `examples/` directory includes real single-file sources from that lineage:

- `examples/VOIDRUNNER.c`
- `examples/nervk.c`

## Quick Start

For a release-style build, pass the C file directly. The runnable is written beside the source file:

```bash
./build_gcc9_bullseye.sh nervk.c
```

That writes:

```text
./nervk
```

The GCC 9 wrapper uses a local Podman image so byte-chasing builds are more repeatable across hosts. The first run builds `localhost/c-optimizer-gcc9:bullseye`; later runs reuse it.

For the default host compiler instead:

```bash
./build_asm_syscall.sh nervk.c
```

For a GUI file picker:

```bash
./run_c_optimizer.sh
```

Double-clickable local launcher:

```bash
./install_desktop.sh
```

After installing the launcher, open **C Optimizer** from your application menu and choose a single C source file.

## Requirements

Required:

- Linux x86_64
- `sh`
- `gcc`
- `strip`
- `readelf`
- `xz`
- `python3`

Optional:

- `sstrip` for external section stripping. If missing, `tiny_tools/sstrip64.py` is used.
- `kdialog` or `zenity` for the graphical file picker.
- `notify-send` for desktop notifications.

## Output

Given:

```text
/home/pingu/projects/demo.c
```

C Optimizer writes:

```text
/home/pingu/projects/demo
```

The output is an executable shell stub with compressed ELF payload appended. Running it extracts the payload to `/tmp`, runs it, and exits with the wrapped program's exit status.

## Try The Example

```bash
./build_gcc9_bullseye.sh examples/nervk.c
```

That writes `examples/nervk`. Generated example runners are ignored by git.

Build both real examples:

```bash
./build_gcc9_bullseye.sh examples/nervk.c
./build_gcc9_bullseye.sh examples/VOIDRUNNER.c
```

To send output somewhere else, set `OUT`:

```bash
OUT=release/nervk ./build_gcc9_bullseye.sh examples/nervk.c
```

## Why GCC 9?

Tiny packed C releases are sensitive to compiler version. The raw stripped ELF can be similar across GCC versions, while the final shell runner changes because x86 BCJ and LZMA compress different instruction layouts differently.

On the current reference machine:

```text
Source      Host GCC 16 runner   GCC 9 Bullseye runner   Saved
nervk       31,841 bytes         31,472 bytes            369 bytes / 1.16%
VOIDRUNNER  33,934 bytes         33,543 bytes            391 bytes / 1.15%
```

GCC 9 was the smallest tested toolchain for VOIDRUNNER and also made the included nervk example smaller. See [Toolchains](docs/TOOLCHAINS.md) for the compiler comparison table and wrapper details.

## Documentation

- [Usage](docs/USAGE.md)
- [Demoscene Workflow](docs/DEMOSCENE-WORKFLOW.md)
- [Examples](docs/EXAMPLES.md)
- [Toolchains](docs/TOOLCHAINS.md)
- [Build Pipeline](docs/BUILD-PIPELINE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

## License

C Optimizer is licensed under the [Apache License, Version 2.0](LICENSE).

Generated runners are output artifacts from your own C source. C Optimizer does not impose Apache-2.0 on the programs you build with it.

## Notes

This is intentionally narrow:

- One C file in, one runnable out.
- No multi-file project discovery.
- No Make/CMake integration.
- Linux x86_64 only.

It is a tiny release-build tool for tiny C productions, not a general C build system.

# C Optimizer

C Optimizer turns a single C source file into a tiny runnable Linux executable wrapper.

It was built for small demo/game-style C projects where the whole project lives in one `.c` file and the output size matters more than conventional build ergonomics.

## What It Does

- Opens a file picker for one `.c` file.
- Builds that file with size-focused GCC flags.
- Uses a tiny custom `_start` instead of normal startup files.
- Strips and section-strips the ELF.
- Searches several x86 BCJ + raw LZMA settings.
- Emits a self-extracting runnable beside the source file.
- Reports output sizes using the best-fit IEC unit plus exact bytes and bits.

The original `.c` file is left untouched.

## Quick Start

CLI:

```bash
./run_c_optimizer.sh /path/to/project.c
```

GUI picker:

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
./run_c_optimizer.sh examples/hello.c
./examples/hello
```

## Documentation

- [Usage](docs/USAGE.md)
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

It is a tiny build tool for tiny C things, not a general C build system.

# Build Pipeline

C Optimizer is a size-focused wrapper around GCC, stripping, and compression.

## 1. Custom Entry Point

`start_syscall.S` defines `_start`.

It:

- Reads `argc`, `argv`, and `envp` from the initial stack.
- Calls C `main(argc, argv, envp)`.
- Exits with the raw Linux `exit` syscall.

This avoids normal C runtime startup files and keeps the executable smaller.

## 2. GCC Compile And Link

`build_asm_syscall.sh` compiles the selected C file with size-focused flags:

- `-Oz`
- `-nostartfiles`
- `-ffunction-sections`
- `-fdata-sections`
- linker garbage collection
- no build id
- no PIE
- no stack protector
- reduced unwind/metadata output

The result is a raw ELF executable before compression.

## 3. Strip And Section Strip

The script runs:

```bash
strip -s
```

Then it uses either:

- `sstrip`, if installed
- `tiny_tools/sstrip64.py`, otherwise

The Python fallback removes section headers and truncates the file to the last loadable segment for normal ELF64 little-endian Linux binaries.

## 4. BCJ + Raw LZMA Search

The stripped ELF is compressed with:

```bash
xz --format=raw --x86 --lzma1=...
```

The script tries combinations of:

- `lc`: `0..4`
- `pb`: `0..2`
- dictionary: `64KiB` through `4MiB`

Each candidate is decompressed and compared against the original stripped ELF. The smallest verified candidate wins.

## 5. Self-Extracting Runner

The final output is a shell script followed by compressed binary data.

At runtime it:

1. Creates a temporary file in `/tmp`.
2. Decompresses the appended payload into that file.
3. Marks it executable.
4. Runs it with the original arguments.
5. Exits with the wrapped program's exit code.

## 6. Budget Report

The builder prints the final runner size and reports whether it is under 32 KiB.

The 32 KiB target is a convenience budget for tiny demos. Larger outputs still run normally.


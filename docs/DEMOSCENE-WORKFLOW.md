# Demoscene Workflow

C Optimizer exists for single-file Linux sizecoding projects: small games, intros, and procedural experiments where the release build is a tiny runnable rather than a normal application bundle.

## Reference Productions

The workflow was shaped by projects like:

- VOIDRUNNER
  - GitHub: <https://github.com/pinguy/VOIDRUNNER>
  - Demozoo: <https://demozoo.org/productions/392804/>
  - Pouet: <https://www.pouet.net/prod.php?which=106445>
- nervk
  - GitHub lineage: <https://github.com/pinguy/murkk>
  - Demozoo: <https://demozoo.org/productions/392800/>
  - Pouet: <https://www.pouet.net/prod.php?which=106436>

These are native Linux procedural game/intros where the interesting work is not just making a file small, but keeping enough actual game or demo inside the size limit to still feel real.

`examples/VOIDRUNNER.c` and `examples/nervk.c` are included so users can build real productions, not only toy programs.

## Good Fit

C Optimizer is a good fit when:

- The production is one `.c` file.
- Assets are generated or encoded in code instead of loaded from disk.
- You want a packed runnable beside the source file.
- You are willing to trade CPU/RAM/startup work for fewer bytes on disk.
- You care about exact byte counts and reproducible release artefacts.

It is a bad fit when:

- The project is a normal multi-file program.
- You need a general dependency manager.
- You need portable Windows/macOS output.
- You expect every SDL/OpenGL/libm symbol to be linked normally and still hit tiny size targets.

## Source Strategy

The projects this targets usually get smaller when the C file is written with compression in mind:

- Keep related code/data close together so LZMA sees repeated patterns.
- Generate textures, levels, meshes, audio, and font data at startup/runtime.
- Prefer deterministic seeds over stored data.
- Use compact tables only when the table beats procedural generation.
- Remove decorative strings, debug text, and duplicate UI labels before release.
- Measure every feature by what it costs in the final runner, not source lines.

## Library Strategy

The size budget normally counts the produced runnable, not system libraries already present on the target Linux machine.

For tiny graphical productions, it can be smaller to use `dlopen`/`dlsym` inside the C source for libraries such as SDL2, OpenGL, or libm instead of linking them conventionally. That can reduce `DT_NEEDED`, PLT, relocation, and symbol baggage in the release artefact.

C Optimizer does not rewrite the source to do that automatically. It gives you the build/pack path; the source still owns the library strategy.

## Release Loop

1. Build normally while developing.
2. Collapse the release into one C file.
3. Run C Optimizer:

   ```bash
   ./run_c_optimizer.sh path/to/production.c
   ```

4. Check the printed sizes:

   ```text
   [build] strip:    ...
   [build] sstrip:   ...
   [build] bcj/lzma: ...
   [build] runner:   ...
   ```

5. Run the generated runner on a clean-ish machine/session.
6. Use `readelf -d` output to check dynamic dependencies.
7. Iterate on the C source if the runner is too large or the dependency list is not what you expected.

For included real examples, see [Examples](EXAMPLES.md).

## What The Final Runner Is

The output file is a shell stub plus compressed native ELF payload.

At runtime it extracts to `/tmp`, marks the payload executable, runs it with the original arguments, then exits with the payload's exit code.

That makes it convenient for demoscene-style release artefacts: one runnable file, no asset directory, no install step.

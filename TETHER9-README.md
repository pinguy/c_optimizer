# TETHER/9 — a tiny orbital salvage game

Original arcade microgame built from scratch for this C Optimizer playground. One `.c` file; no textures, samples, fonts, or level assets on disk. Generated starfield, in-game graphics, glyphs, procedural audio, hostile drones and a spring-cable physics model.

**Objective:** Fly out to amber cargo pods, press **Space** to tether one, and drag it into the blue `DOCK` circle at the centre. Don't let the hunter drones grind down your hull. Completing a wave restores some hull and increases the cargo quota and danger. EMP pulses push enemies away; boosts trade energy for acceleration.

## Run

For the standalone **complete kit**, run from its extracted folder:

```sh
chmod +x ./tether9
./tether9
```

For the **C Optimizer edition**, run from the repository root instead. Linux x86_64 needs the SDL2 runtime, an OpenGL compatibility context, `xz`, and `libm`/`libc`:

```sh
chmod +x examples/tether9
./examples/tether9
```

* `W` thrust; `A` / `D` turn; `S` brake
* `Space` attach/detach the **nearest cargo within 100 world units**
* `E` electromagnetic pulse (32 energy, 5.5-second cooldown)
* `Shift` boost while thrusting (drains energy)
* `Enter` launch; `R` retry after death; `F11` fullscreen; `Esc` quit

The HUD spells out resource names; colour alone is not required to read the status.

## Build

The standalone kit includes `./BUILD_UNPACKED.sh`, which compiles an ordinary larger ELF for development. To rebuild the **size-optimised** 8,023-byte runner, use the full C Optimizer edition and its build script.

From the C Optimizer root:

```sh
./BUILD_TETHER9.sh
```

This invokes the **real C Optimizer** build, including the optional source pre-pass, custom assembly start, `sstrip`, x86 BCJ and LZMA parameter search. Uses host GCC rather than the Podman-based GCC9 path. The output is `examples/tether9`.

Or explicitly:

```sh
COPT_EXTRA_LDLIBS='-ldl -lm' OUT=examples/tether9 \
  ./build_asm_syscall.sh examples/TETHER9.c
```

### Measured in the assistant's Linux container (GCC 14.2)

| Stage | Bytes |
| --- | ---: |
| Stripped ELF | 18,040 |
| Section-stripped ELF | 16,812 |
| BCJ + LZMA payload | 7,875 |
| **Final runnable** | **8,023** |
| Headroom below 8192 bytes | **169** |

Build size is compiler/toolchain-dependent and **will differ on other machines**. The shell wrapper uses C Optimizer's existing `/tmp/v$$` extraction; it is a sizecoding launcher, **not a hardened secure loader**. Use only in a trusted local environment. Mounts with `noexec` on `/tmp` will prevent execution.

## Tests

From the C Optimizer root:

```sh
# Game logic (14 assertions), no graphics device needed
gcc -O2 -DTEST_BUILD -I compat examples/TETHER9.c -ldl -lm -o /tmp/tether9-test
/tmp/tether9-test --selftest

# Prepass and font orientation regressions (3 cases)
python3 -m unittest discover -s tests -v

# Play an entire first wave with the unmodified game physics
# and a reproducible deterministic steering controller:
gcc -O2 -I compat tests/play_one_wave.c -ldl -lm -o /tmp/play-one-wave
/tmp/play-one-wave 12345

# An automated gameplay/render smoke session (needs display or Xvfb)
./examples/tether9 --demo --frames 120 --seed 12345
```

A separate debug build can capture its framebuffer as a PPM file for inspection:

```sh
gcc -O2 -DCAPTURE_BUILD -I compat examples/TETHER9.c -ldl -lm -o /tmp/tether9-preview
/tmp/tether9-preview --demo --frames 85 --capture /tmp/tether9.ppm
```

The shipped 8 KiB runner intentionally omits the screenshot-capture code.

## Font fix and complete playthrough (8 October 2026)

The initial 8,025-byte build included an upside-down numeric font. Its A–Z and punctuation glyphs used top-first bitmaps, but its decimal digits were recorded bottom-first. The corrected source reverses the seven bits in each numeric glyph, so the `9` in the title, wave number, score and salvage counter now face the right way. This version also adds `tests/test_tether9_digits.py` to prevent regressions.

A full first-wave test was completed in the **running SDL game** under Xvfb. An external Python controller sent actual XTEST keyboard events (W/A/D/S/Space/E), with a separate debug-only build writing read-only positions to guide steering. It physically recovered four pods, completed wave 1, advanced to wave 2, and reached **925 score**. The hull was **88.2 immediately before wave completion** and returned to 100 after the built-in wave repair. See `wave2.png`; release gameplay code is unchanged except for glyph data. The new ~8 KiB packed runner was also started separately and captured as `preview.png`.

The included `tests/play_one_wave.c` uses the same `step()`, tether and EMP functions without a display, and completes a full wave deterministically for regression testing; it is a companion test, not code included in the release game.

## Scope and limitations

This is a tiny proof-of-playability, not a shipped commercial game. The game logic was unit-tested, an automated preview ran with software OpenGL, and the release runner was executed successfully under Xvfb in the assistant's container. An automated controller completed a full level through actual keyboard inputs; human subjective playtesting and listening on real speakers were not performed here. The SDL audio queue was exercised with the dummy audio device.

The source pre-pass also got a conservative fix: it now preserves `<string.h>` whenever functions such as `strcmp` or `strlen` are still used. The two included regression tests cover this and the safe removal case. None of the original sample games were edited.

TETHER/9 source: `SPDX-License-Identifier: MIT`.

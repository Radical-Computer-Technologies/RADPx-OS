# RADixKernel/boards — per-board bring-up source

Board-specific kernel entry, early boot assembly, and link scripts. This is RADPx
*source* (previously under `tools/embedded/`); the build glue (Makefiles/CMake/
smoke scripts) stays in `tools/embedded/<target>/` and references these files.

- `zuboard_1cg/` — Avnet/Tria ZuBoard-1CG (ZynqMP A53): `kernel.cpp` (board entry
  + device-tree/service/self-test wiring), `boot.S` (EL1 vectors, trap frames,
  PSCI secondary start), `linker.ld`.

Other targets (Pi Zero 2 W, legacy x86 GRUB) still keep their entry source under
`tools/embedded/` pending the same migration.

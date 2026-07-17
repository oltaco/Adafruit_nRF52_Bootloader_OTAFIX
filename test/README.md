# Bootloader `.mota` apply — host simulation test

Validates the nRF52 in-place delta apply (`src/ota_delta.c` + the vendored `detools` decoder + `sha256`)
**on the host, without hardware**. It compiles the real `ota_delta_check_and_apply()` with the
`OTA_DELTA_HOST_TEST` shims, lays out a RAM "flash" exactly like the device (running image at `APP_BASE`,
a staged `.mota` bottom-aligned below `FS_START`, `GPREGRET` set), runs the apply, and checks the result
against the expected new image. Every decision point (EndF scan, `.mota` scan, base check, detools return
code, post-hash) is printed, so a failing apply is debuggable here instead of via flash-and-pray on a board.

## Run

```bash
make check        # apply the committed vector (apply_sim) + the LTO-readback regression (readback_test)
```

Debug an arbitrary scenario (e.g. the real firmware that misbehaved on a device):

```bash
make apply_sim
./apply_sim <base.img> <delta.mota> <expected_new.img>
```

`base.img` / `expected_new.img` are flat app images (`BODY||EndF`, what lives at `APP_BASE`); for an nRF52
build you can get one from `firmware.hex` with `intelhex` (`ih.tobinarray(start=ih.minaddr())`). Build the
`.mota` with `tools/motatool` (in the MeshCore repo).

## Why this exists

Real-hardware apply debugging is slow and brick-prone (reflash + LoRa fetch per iteration). This harness
caught a real bug: in-place deltas were built with `--inplace-memory = FS_START - APP_BASE` (`0xAE000`),
but the apply workspace is `[APP_BASE, mota_addr)` — the staged `.mota` sits *inside* that span, so detools
overran the workspace and returned `DETOOLS_IO_FAILED`; the device just rebooted with nothing applied. The
correct value (`MOTA_NRF52_INPLACE_MEMORY = 0x98000`, leaving room below `FS_START` for the staged `.mota`)
makes the apply succeed — proven here in seconds. To exercise that workspace boundary, run `apply_sim` with
a realistic (~550 KB) image; the tiny committed vector validates the apply *pipeline* (parse / scan / base
check / detools decode / result hash).

## `readback_test` — the `-flto` flash-readback regression

A second, HW-confirmed bug: in-place apply *reads back flash it just wrote* (the output overlaps the
input). The write goes through `nrfx_nvmc_words_write`; the readback through `fl_read` →
`memcpy(dst, (const void*)(uintptr_t)addr, n)`. Same flash, two different pointer provenances — so
whole-program `-flto` decides they can't alias and caches a **stale** read, the post-hash sees pre-decode
bytes, and the apply is silently refused (the old firmware boots). The fix: `fl_read` reads through a
`volatile` pointer. `-fno-strict-aliasing` does *not* cover it (provenance, not type aliasing).

A plain host run can't reproduce the miscompile — here `otah_read`/`otah_write_words` hit the *same* C
array, an obvious alias the compiler never gets wrong. So `readback_test` guards it three ways:

1. **positive** — coherent readback ⇒ the apply succeeds, commits, and matches the expected image.
2. **negative** — it *injects* the exact failure mode (workspace reads return stale pre-write bytes) and
   asserts the apply **fails safe**: nothing committed, returns false (→ DFU, never a corrupt boot).
3. **source guard** — asserts the device `fl_read` still reads through `volatile`. This is the only check
   that catches a "someone reverted the fix" regression (1/2 can't, on the host). Verified: flipping
   `fl_read` back to a plain `memcpy` turns the suite red.

## Regenerating the committed vector

`test/vectors/{base.img,new.img,delta.mota}` are built from the MeshCore reference (`tools/mota/motalib`)
with a small synthetic firmware and an in-place delta (`memory_size = 0x98000`, `segment_size = 4096`,
`crle`). Regenerate if the `.mota`/EndF format changes.

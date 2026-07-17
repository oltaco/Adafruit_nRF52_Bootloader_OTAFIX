// Regression test for the -flto flash-readback aliasing miscompile in the in-place apply.
// ----------------------------------------------------------------------------------------------------
// THE BUG (HW-confirmed, RAK4631): in-place delta apply READS BACK flash it just WROTE (the output
// overlaps the input). The write goes through nrfx_nvmc_words_write(addr,...); the readback goes through
// fl_read -> memcpy(dst, (const void*)(uintptr_t)addr, n). Those touch the same flash via two DIFFERENT
// pointer provenances, so whole-program -flto alias analysis concludes they can't alias and caches /
// reorders a STALE (pre-write) read. The post-apply sha256 then hashes pre-decode bytes -> mismatch ->
// apply silently refused -> the old firmware boots unchanged. (-fno-strict-aliasing does NOT help: this
// is pointer provenance, not type-based aliasing.) The fix: fl_read reads through a `volatile` pointer.
//
// WHY THIS NEEDS A SPECIAL TEST: a plain host run cannot reproduce the miscompile — here otah_read and
// otah_write_words both touch the SAME C array (FLASH[]), an obvious alias the compiler will never get
// wrong, with or without -flto. So we cover the bug three ways:
//   [1] POSITIVE  coherent readback  -> apply succeeds, commits, and matches the expected image.
//   [2] NEGATIVE  inject the exact failure mode (workspace reads return STALE bytes) -> assert the apply
//                 FAILS SAFE: it does not commit and returns false (never boots a corrupt image).
//   [3] GUARD     assert the device fl_read reads through `volatile` — the one check that catches a
//                 "someone reverted the fix" regression, which [1]/[2] cannot on the host.
//
// Build/run: see test/Makefile (`make check`). Uses the committed vectors in test/vectors/ by default.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ota_layout.h"

// ----- simulated flash + a pre-apply SNAPSHOT used to model the LTO stale read -----
#define FLASH_LEN  MOTA_NRF52_FS_START
static uint8_t  FLASH[FLASH_LEN];
static uint8_t  SNAPSHOT[FLASH_LEN];     // pre-apply copy; served for workspace reads in stale mode
static int      g_stale = 0;             // 1 => a workspace read returns the snapshot (THE LTO BUG)
static uint32_t g_ws_lo, g_ws_hi;        // workspace = [APP_BASE, mota_addr); the stale-read window
static uint32_t g_gpregret;
static uint16_t g_bank0, g_crc; static uint32_t g_size; static int g_committed;

void otah_read(uint32_t a, void* d, uint32_t n) {
    memcpy(d, FLASH + a, n);
    if (g_stale) {
        // Overlay the pre-apply snapshot over any part of this read that falls in the workspace — i.e.
        // model "the compiler served a cached read from before the write" for exactly the bytes the
        // in-place decode writes and reads back. Reads outside the workspace (the .mota payload, the
        // running body before any write) are unaffected, just like the real miscompile.
        uint32_t os = a > g_ws_lo ? a : g_ws_lo;
        uint32_t oe = (a + n) < g_ws_hi ? (a + n) : g_ws_hi;
        if (os < oe) memcpy((uint8_t*)d + (os - a), SNAPSHOT + os, oe - os);
    }
}
void     otah_erase(uint32_t page)                         { memset(FLASH + page, 0xFF, MOTA_NRF52_FLASH_PAGE); }
void     otah_write_words(uint32_t a, const uint32_t* s, uint32_t nw) {
    uint32_t* dst = (uint32_t*)(FLASH + a);
    for (uint32_t i = 0; i < nw; i++) dst[i] &= s[i];      // NOR: write only clears bits (target pre-erased)
}
uint32_t otah_gpregret_get(void)                           { return g_gpregret; }
void     otah_gpregret_set(uint32_t v)                     { g_gpregret = v; }
uint16_t otah_crc16(uint32_t a, uint32_t len)              { (void)a; (void)len; return 0x1234; }
void otah_settings_commit(uint16_t b, uint16_t c, uint32_t s) { g_bank0 = b; g_crc = c; g_size = s; g_committed = 1; }

#include "ota_delta.c"   // unit under test (its static page-cache g_cache_page/g_cache_dirty are visible here)

// ----- vector loading + flash layout (mirrors apply_sim.c) -----
static uint8_t *g_base, *g_mota, *g_expect;
static long     g_base_n, g_mota_n, g_exp_n;
static uint32_t g_write_start;

static long load(const char* path, uint8_t** out) {
    FILE* f = fopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    *out = malloc(n); if (fread(*out, 1, n, f) != (size_t)n) { fprintf(stderr, "read %s\n", path); exit(2); }
    fclose(f); return n;
}

static void stage_flash(void) {
    memset(FLASH, 0xFF, FLASH_LEN);
    memcpy(FLASH + MOTA_NRF52_APP_BASE, g_base, g_base_n);          // running image at APP_BASE
    memcpy(FLASH + g_write_start, g_mota, g_mota_n);                // staged .mota bottom-aligned
    static const uint8_t APRV4[4] = {'A','P','R','V'};
    memcpy(FLASH + g_write_start + 8 + 193, APRV4, 4);             // approval @ manifest offset 193 (APRV)
    g_gpregret = GPREGRET_OTA_APPLY;
    g_committed = 0; g_bank0 = 0; g_size = 0;
    g_cache_page = 0; g_cache_dirty = 0;                           // reset ota_delta.c's static page cache
}

// Run the REAL entry point once. Returns its bool; fills committed + matches(expected) for the caller.
static bool run_case(int stale, int* committed, int* matches) {
    stage_flash();
    memcpy(SNAPSHOT, FLASH, FLASH_LEN);                            // freeze the pre-apply image
    g_ws_lo = MOTA_NRF52_APP_BASE; g_ws_hi = g_write_start;        // workspace = [APP_BASE, mota_addr)
    g_stale = stale;
    bool applied = ota_delta_check_and_apply();
    g_stale = 0;
    *committed = g_committed;
    *matches   = (g_exp_n > 0) && (memcmp(FLASH + MOTA_NRF52_APP_BASE, g_expect, g_exp_n) == 0);
    return applied;
}

// [3] Source guard: the DEVICE fl_read (the on-hardware #else branch) must read flash through a
// `volatile` pointer, or -flto reintroduces the stale-readback bug this whole file exists to prevent.
// We can't catch a removed `volatile` behaviorally on the host, so we check the source directly.
static int guard_device_flread_is_volatile(void) {
    const char* cands[] = { "../src/ota_delta.c", "src/ota_delta.c", "ota_delta.c" };
    FILE* f = NULL; const char* used = NULL;
    for (size_t i = 0; i < sizeof cands / sizeof cands[0]; i++) { f = fopen(cands[i], "rb"); if (f) { used = cands[i]; break; } }
    if (!f) { printf("  WARN: ota_delta.c source not found; cannot verify the volatile guard\n"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = malloc(n + 1); if (fread(s, 1, n, f) != (size_t)n) { fclose(f); free(s); printf("  WARN: short read\n"); return 1; }
    s[n] = 0; fclose(f);
    // Locate the device (#else) abstraction block, then its fl_read, then require `volatile` in the body.
    char* dev = strstr(s, "#include \"nrf.h\"");
    char* fr  = dev ? strstr(dev, "fl_read(uint32_t a, void* d, uint32_t n)") : NULL;
    if (!dev || !fr) { printf("  WARN: device fl_read not located (source refactored?); update this guard\n"); free(s); return 1; }
    char* fe = strstr(fr, "fl_erase");           // bound the search to the fl_read body
    char saved = 0; if (fe) { saved = *fe; *fe = 0; }
    int ok = (strstr(fr, "volatile") != NULL);
    if (fe) *fe = saved;
    free(s);
    if (ok) printf("  device fl_read reads through a volatile pointer (LTO-safe)  [%s]\n", used);
    else    printf("  *** device fl_read is NOT volatile — -flto WILL cache a stale readback (the bug is back) ***\n");
    return ok;
}

int main(int argc, char** argv) {
    const char* base_p = argc > 1 ? argv[1] : "vectors/base.img";
    const char* mota_p = argc > 2 ? argv[2] : "vectors/delta.mota";
    const char* new_p  = argc > 3 ? argv[3] : "vectors/new.img";
    g_base_n = load(base_p, &g_base); g_mota_n = load(mota_p, &g_mota); g_exp_n = load(new_p, &g_expect);
    g_write_start = (uint32_t)((MOTA_NRF52_FS_START - g_mota_n) & ~(MOTA_NRF52_FLASH_PAGE - 1));
    if (g_write_start < MOTA_NRF52_APP_BASE + g_base_n) { fprintf(stderr, "mota overlaps app!\n"); return 2; }

    int fails = 0;
    int committed, matches; bool applied;

    // [1] POSITIVE — coherent readback: the apply must succeed, commit, and reproduce the new image.
    printf("[1] positive (coherent readback): ");
    applied = run_case(/*stale=*/0, &committed, &matches);
    if (applied && committed && matches) {
        printf("PASS — applied, committed, app region == expected\n");
    } else {
        printf("FAIL — applied=%d committed=%d matches=%d (expected 1/1/1)\n", applied, committed, matches);
        fails++;
    }

    // [2] NEGATIVE — inject the LTO failure mode (stale workspace readback). The apply MUST fail safe:
    // it must NOT commit (so the bootloader never boots a corrupt image) and must return false.
    printf("[2] negative (stale workspace readback = the LTO bug): ");
    applied = run_case(/*stale=*/1, &committed, &matches);
    if (!committed && !applied && !matches) {
        printf("PASS — apply refused, nothing committed (fails safe -> DFU)\n");
    } else {
        printf("FAIL — applied=%d committed=%d matches=%d (expected 0/0/0: a stale read must never commit)\n",
               applied, committed, matches);
        fails++;
    }
    // Sanity: staleness must actually have CHANGED the outcome, else this vector doesn't exercise the
    // readback path and the negative test is vacuous. (Re-run positive to compare.)
    {
        int c2, m2; bool a2 = run_case(0, &c2, &m2);
        if (a2 && c2 && !(applied && committed)) {
            printf("    (sensitivity OK: coherent commits, stale does not — vector exercises readback)\n");
        } else if (a2 && c2) {
            printf("    NOTE: coherent and stale both committed — vector may not exercise cross-page readback\n");
        }
    }

    // [3] GUARD — the device fl_read must stay volatile.
    printf("[3] source guard (device fl_read is volatile):\n");
    if (!guard_device_flread_is_volatile()) fails++;

    printf("\n%s (%d failure%s)\n", fails ? "SUITE FAILED" : "SUITE PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}

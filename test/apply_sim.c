// Host simulation of the nRF52 bootloader's .mota in-place apply — NO hardware needed.
//
// Lays out a RAM "flash" exactly like the device (running image at APP_BASE, a staged .mota bottom-aligned
// below FS_START, GPREGRET set), then runs the REAL ota_delta_check_and_apply() and checks the result
// against the expected new image. Prints each decision point (EndF scan, .mota scan, base check, apply,
// post-hash) so a failing apply is debuggable on the host instead of via flash-and-pray on hardware.
//
//   apply_sim <base.img> <delta.mota> <expected_new.img>
//     base.img          the running firmware image (BODY||EndF), placed at APP_BASE
//     delta.mota         the in-place delta container (approval is forced to APRV here, as the app does)
//     expected_new.img  what the app region must equal after a successful apply
//
// Build: see test/Makefile  (gcc, -DOTA_DELTA_HOST_TEST).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ota_layout.h"

#define FLASH_LEN  MOTA_NRF52_FS_START          // [0, FS_START) is enough (settings/MBR are stubbed)
static uint8_t  FLASH[FLASH_LEN];
static uint32_t g_gpregret;
static uint16_t g_bank0, g_crc; static uint32_t g_size; static int g_committed;

void     otah_read(uint32_t a, void* d, uint32_t n)        { memcpy(d, FLASH + a, n); }
void     otah_erase(uint32_t page)                         { memset(FLASH + page, 0xFF, MOTA_NRF52_FLASH_PAGE); }
void     otah_write_words(uint32_t a, const uint32_t* s, uint32_t nw) {
    uint32_t* dst = (uint32_t*)(FLASH + a);
    for (uint32_t i = 0; i < nw; i++) dst[i] &= s[i];      // NOR: write only clears bits (target pre-erased)
}
uint32_t otah_gpregret_get(void)                           { return g_gpregret; }
void     otah_gpregret_set(uint32_t v)                     { g_gpregret = v; }
uint16_t otah_crc16(uint32_t a, uint32_t len)              { (void)a; (void)len; return 0x1234; }
void otah_settings_commit(uint16_t b, uint16_t c, uint32_t s) { g_bank0 = b; g_crc = c; g_size = s; g_committed = 1; }

#include "ota_delta.c"   // unit under test (statics — scan_mota/find_body_len/parse_mota_at — visible here)

static long load(const char* path, uint8_t** out) {
    FILE* f = fopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    *out = malloc(n); if (fread(*out, 1, n, f) != (size_t)n) { fprintf(stderr, "read %s\n", path); exit(2); }
    fclose(f); return n;
}
static void hex8(const uint8_t* p) { for (int i = 0; i < 8; i++) printf("%02x", p[i]); }

int main(int argc, char** argv) {
    if (argc != 4) { fprintf(stderr, "usage: apply_sim <base.img> <delta.mota> <expected_new.img>\n"); return 2; }
    uint8_t *base, *mota, *expect;
    long base_n = load(argv[1], &base), mota_n = load(argv[2], &mota), exp_n = load(argv[3], &expect);

    memset(FLASH, 0xFF, FLASH_LEN);
    if (MOTA_NRF52_APP_BASE + base_n > FLASH_LEN) { fprintf(stderr, "base too big\n"); return 2; }
    memcpy(FLASH + MOTA_NRF52_APP_BASE, base, base_n);                       // running image at APP_BASE

    // stage the .mota bottom-aligned below FS_START, page-aligned (mirrors OtaStoreFlashNrf52)
    uint32_t write_start = (uint32_t)((MOTA_NRF52_FS_START - mota_n) & ~(MOTA_NRF52_FLASH_PAGE - 1));
    if (write_start < MOTA_NRF52_APP_BASE + base_n) { fprintf(stderr, "mota overlaps app!\n"); return 2; }
    memcpy(FLASH + write_start, mota, mota_n);
    // the app writes APRV into the staged manifest's approval field before reset — do the same here
    static const uint8_t APRV4[4] = {'A','P','R','V'};
    memcpy(FLASH + write_start + 8 + 193, APRV4, 4);   // approval @ manifest offset 193 (fixed layout)
    g_gpregret = GPREGRET_OTA_APPLY;

    printf("== layout ==\n  APP_BASE=0x%X base=%ld bytes (ends 0x%lX)\n  FS_START=0x%X  mota=%ld bytes @0x%X\n",
           MOTA_NRF52_APP_BASE, base_n, MOTA_NRF52_APP_BASE + base_n, MOTA_NRF52_FS_START, mota_n, write_start);

    // ---- step-by-step diagnostics (the static internals) ----
    uint32_t body_len = 0;
    int fb = find_body_len(&body_len);
    printf("== find_body_len ==\n  ok=%d body_len=%u\n", fb, body_len);
    if (fb) { uint8_t h[32]; sha256_region(MOTA_NRF52_APP_BASE, body_len, h);
              printf("  sha256:8(running body)= "); hex8(h); printf("\n"); }

    struct mota_min m; memset(&m, 0, sizeof m);
    uint32_t found = scan_mota(&m);
    printf("== scan_mota ==\n  found=0x%X (expected 0x%X)\n", found, write_start);
    if (found) {
        printf("  total=%u image_size=%u payload_size@addr=0x%X approval=0x%X approved=%d\n",
               m.total, m.image_size, m.payload_addr, m.approval_addr, m.approved);
        printf("  is_full=%d codec_id=%d  base_hash(mota)= ", m.is_full, m.codec_id); hex8(m.base_hash); printf("\n");
    }

    // ---- instrumented apply (mirrors ota_delta_check_and_apply's destructive step) ----
    printf("== in-place apply ==\n");
    struct apply_ctx c;
    c.patch_addr = m.payload_addr; c.patch_len = m.payload_size; c.patch_pos = 0;
    c.ws_lo = MOTA_NRF52_APP_BASE; c.ws_hi = found; c.step = 0;     // workspace = [APP_BASE, mota_addr)
    printf("  workspace=[0x%X,0x%X)=%u bytes  patch=%u bytes\n", c.ws_lo, c.ws_hi, c.ws_hi - c.ws_lo, m.payload_size);
    int r = detools_apply_patch_in_place_callbacks(dt_mr, dt_mw, dt_me, dt_ss, dt_sg, dt_pr,
                                                   (size_t)m.payload_size, &c);
    cache_flush();
    printf("  detools returned r=%d  (expected image_size=%u)\n", r, m.image_size);
    int ok = 0;
    if (r >= 0 && (uint32_t)r == m.image_size) {
        uint8_t h[32]; sha256_region(MOTA_NRF52_APP_BASE, m.image_size, h);
        printf("  result sha256:8 = "); hex8(h);
        printf("   manifest image_hash:8 = "); hex8(m.image_hash); printf("\n");
        ok = (memcmp(FLASH + MOTA_NRF52_APP_BASE, expect, exp_n) == 0) && ((long)m.image_size == exp_n);
        if (!ok) for (long i = 0; i < exp_n; i++) if (FLASH[MOTA_NRF52_APP_BASE + i] != expect[i]) {
            printf("  FIRST DIFF at image offset %ld: got 0x%02x want 0x%02x\n", i, FLASH[MOTA_NRF52_APP_BASE + i], expect[i]); break;
        }
    }
    printf("\n%s\n", ok ? "RESULT: APPLY OK — app region == expected new image" : "RESULT: APPLY FAILED");
    return ok ? 0 : 1;
}

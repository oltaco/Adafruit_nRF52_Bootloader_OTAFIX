// MeshCore `.mota` delta-apply for the nRF52 bootloader (single-slot, detools in-place).
// See ota_delta.h for the contract. Compiles for the device (nrfx/SDK) and for a host test harness
// (OTA_DELTA_HOST_TEST: the test TU provides the otah_* flash/settings/gpregret stubs + crc16).
#include "ota_delta.h"
#include "ota_layout.h"
#include "ota_bl_info.h"
#include "detools/detools.h"
#include <string.h>
#include <stdint.h>

// Capability marker the running MeshCore app scans for (see ota_bl_info.h). `used` + the reference in
// ota_delta_check_and_apply() keep it through -ffunction/data-sections, --gc-sections and -flto.
__attribute__((used)) const mota_bl_info_t g_mota_bl_info = {
  { MOTA_BL_MAGIC0, MOTA_BL_MAGIC1, MOTA_BL_MAGIC2, MOTA_BL_MAGIC3,
    MOTA_BL_MAGIC4, MOTA_BL_MAGIC5, MOTA_BL_MAGIC6, MOTA_BL_MAGIC7 },
  MOTA_BL_APPLY_ABI,
  (uint16_t)(1u << 2),                 // this bootloader applies in-place (codec_id 2) deltas
  { 0, 0, 0, 0 },
};

// ---- `.mota` / EndF on-wire constants (mirror of src/helpers/ota/OtaFormat.h, C-friendly) ---------
static const uint8_t MAGIC[4]    = { 'm','O','T','A' };
static const uint8_t TRAILER[5]  = { 'v','k','4','9','6' };
static const uint8_t ENDF[4]     = { 'E','n','d','F' };
static const uint8_t APRV[4]     = { 'A','P','R','V' };
#define ENDF_LEN          56u    // fixed trailer: marker(4)+body_len(4)+body_hash8(8)+fw_ver(4)+target(4)+hw_id(32)
#define MOTA_MFL          197u   // fixed manifest-minus-leaves length (head 89 + base_hash 8 + signer 32 + sig 64 + approval 4)
#define MFLAG_FULL        0x01u
#define MFLAG_SIGNED      0x02u
#define CODEC_INPLACE     2u
#define PAGE              MOTA_NRF52_FLASH_PAGE

// ---- platform flash / settings / gpregret abstraction --------------------------------------------
#ifdef OTA_DELTA_HOST_TEST
  extern void     otah_read(uint32_t addr, void* dst, uint32_t n);
  extern void     otah_erase(uint32_t page_addr);
  extern void     otah_write_words(uint32_t addr, const uint32_t* src, uint32_t nwords);
  extern uint32_t otah_gpregret_get(void);
  extern void     otah_gpregret_set(uint32_t v);
  extern uint16_t otah_crc16(uint32_t addr, uint32_t len);
  extern void     otah_settings_commit(uint16_t bank0, uint16_t crc, uint32_t size);
  #define APP_BASE        MOTA_NRF52_APP_BASE
  static void     fl_read(uint32_t a, void* d, uint32_t n)            { otah_read(a, d, n); }
  static void     fl_erase(uint32_t page)                            { otah_erase(page); }
  static void     fl_write_words(uint32_t a, const uint32_t* s, uint32_t nw) { otah_write_words(a, s, nw); }
  static uint32_t gpregret_get(void)                                 { return otah_gpregret_get(); }
  static void     gpregret_set(uint32_t v)                           { otah_gpregret_set(v); }
  static void     gpregret2_set(uint32_t v)                          { (void)v; }   // diag no-op on host
  static uint16_t crc16_region(uint32_t a, uint32_t len)             { return otah_crc16(a, len); }
#else
  #include "nrf.h"
  #include "nrfx_nvmc.h"
  #include "crc16.h"
  #include "bootloader_types.h"
  #include "bootloader_settings.h"
  #include "dfu_types.h"
  #define APP_BASE        ((uint32_t)DFU_BANK_0_REGION_START)
  // Read flash through a VOLATILE pointer. In-place apply WRITES flash (nrfx_nvmc_words_write) and then
  // READS IT BACK here (decode readback + the post-apply sha256). Those touch the same flash through two
  // different pointer provenances (an integer-cast read pointer vs the nrfx write), so whole-program -flto
  // alias analysis concludes they can't alias and caches/reorders a STALE read — the post-check then hashes
  // pre-decode bytes -> mismatch -> apply silently refused. (-fno-strict-aliasing does NOT help: this is
  // provenance, not type-based aliasing.) The host harness can't reproduce it: there read+write hit the
  // same C array, an obvious alias. volatile forces the actual load each time.
  static void     fl_read(uint32_t a, void* d, uint32_t n) {
    const volatile uint8_t* s = (const volatile uint8_t*)(uintptr_t)a; uint8_t* o = (uint8_t*)d;
    for (uint32_t i = 0; i < n; i++) o[i] = s[i];
  }
  static void     fl_erase(uint32_t page)                            { nrfx_nvmc_page_erase(page); }
  static void     fl_write_words(uint32_t a, const uint32_t* s, uint32_t nw) { nrfx_nvmc_words_write(a, s, nw); }
  static uint32_t gpregret_get(void)                                 { return NRF_POWER->GPREGRET; }
  static void     gpregret_set(uint32_t v)                           { NRF_POWER->GPREGRET = v; }
  // Diagnostic: stash an apply bail/progress code in GPREGRET2 (retained across the boot to the app, which
  // reads it back). SD is off in the bootloader, so a direct write is fine.
  static void     gpregret2_set(uint32_t v)                          { NRF_POWER->GPREGRET2 = v; }
  static uint16_t crc16_region(uint32_t a, uint32_t len)             { return crc16_compute((const uint8_t*)(uintptr_t)a, len, NULL); }
  static void otah_settings_commit(uint16_t bank0, uint16_t crc, uint32_t size) {
    bootloader_settings_t s; const bootloader_settings_t* cur;
    bootloader_util_settings_get(&cur); memcpy(&s, cur, sizeof(s));
    s.bank_0 = bank0; s.bank_0_crc = crc; s.bank_0_size = size;
    nrfx_nvmc_page_erase(BOOTLOADER_SETTINGS_ADDRESS);
    nrfx_nvmc_words_write(BOOTLOADER_SETTINGS_ADDRESS, (const uint32_t*)&s, sizeof(s) / 4);
  }
  #define BANK_VALID_APP_V  0x01u
#endif
#ifdef OTA_DELTA_HOST_TEST
  #define BANK_VALID_APP_V  0x01u
#endif

#include "sha256.h"

static uint32_t rd_u32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Tiny bounds-checked cursor so the manifest is parsed by reading each field by name in order, instead of
// hand-computed offsets (mirrors MeshCore's OtaByteIO.h ByteReader). Any over-read flips ok=0.
typedef struct { const uint8_t* p; uint32_t len, n; int ok; } br_t;
static uint8_t  br_u8(br_t* r)  { if (r->ok && (uint64_t)r->n + 1 <= r->len) return r->p[r->n++]; r->ok = 0; return 0; }
static uint32_t br_u32(br_t* r) { if (r->ok && (uint64_t)r->n + 4 <= r->len) { uint32_t v = rd_u32(r->p + r->n); r->n += 4; return v; } r->ok = 0; return 0; }
static const uint8_t* br_take(br_t* r, uint32_t k) { if (r->ok && (uint64_t)r->n + k <= r->len) { const uint8_t* x = r->p + r->n; r->n += k; return x; } r->ok = 0; return NULL; }
static void br_skip(br_t* r, uint32_t k) { if (r->ok && (uint64_t)r->n + k <= r->len) r->n += k; else r->ok = 0; }

static void sha256_region(uint32_t addr, uint32_t len, uint8_t out[32]) {
  sha256_ctx_t c; sha256_init(&c);
  uint8_t buf[256];
  while (len) { uint32_t n = len < sizeof(buf) ? len : sizeof(buf); fl_read(addr, buf, n); sha256_update(&c, buf, n); addr += n; len -= n; }
  sha256_final(&c, out);
}

// ---- coherent single-page write-back cache (in-place reads back shifted data it just wrote) -------
static uint8_t  g_cache[PAGE];
static uint32_t g_cache_page;                   // page-aligned addr; 0 == INVALID (no app page is at 0)
static int      g_cache_dirty;

static void cache_flush(void) {
  if (!g_cache_page) return;
  if (g_cache_dirty) { fl_erase(g_cache_page); fl_write_words(g_cache_page, (const uint32_t*)g_cache, PAGE / 4); }
  g_cache_page = 0; g_cache_dirty = 0;
}
static void cache_use(uint32_t page) {
  if (g_cache_page == page) return;
  cache_flush();
  g_cache_page = page; g_cache_dirty = 0;
  fl_read(page, g_cache, PAGE);
}
static void cread(uint32_t addr, uint8_t* dst, uint32_t n) {     // coherent read (overlay dirty page)
  fl_read(addr, dst, n);
  if (g_cache_page && g_cache_dirty) {
    uint32_t cs = g_cache_page, ce = g_cache_page + PAGE, a = addr, e = addr + n;
    uint32_t os = a > cs ? a : cs, oe = e < ce ? e : ce;
    if (os < oe) memcpy(dst + (os - a), g_cache + (os - cs), oe - os);
  }
}
static void cwrite(uint32_t addr, const uint8_t* src, uint32_t n) {
  while (n) {
    uint32_t page = addr & ~(PAGE - 1), off = addr - page, chunk = PAGE - off;
    if (chunk > n) chunk = n;
    cache_use(page); memcpy(g_cache + off, src, chunk); g_cache_dirty = 1;
    addr += chunk; src += chunk; n -= chunk;
  }
}
static void cerase(uint32_t addr, uint32_t n) {                  // detools calls this page-aligned
  while (n) {
    uint32_t page = addr & ~(PAGE - 1), off = addr - page, step = PAGE - off;
    if (step > n) step = n;
    if (g_cache_page == page) { memset(g_cache, 0xFF, PAGE); g_cache_dirty = 1; }
    else fl_erase(page);
    addr += step; n -= step;
  }
}

// ---- detools in-place callbacks (region addresses are 0-based; base sits at workspace offset 0) ---
struct apply_ctx {
  uint32_t patch_addr, patch_len, patch_pos;
  uint32_t ws_lo, ws_hi;        // workspace = [ws_lo, ws_hi); ws_hi == mota start (never written)
  int step;
};
static int dt_mr(void* a, void* dst, uintptr_t src, size_t n) {
  struct apply_ctx* c = a; uint32_t addr = c->ws_lo + (uint32_t)src;
  if (addr + n > c->ws_hi) return -DETOOLS_IO_FAILED;
  cread(addr, dst, n); return DETOOLS_OK;
}
static int dt_mw(void* a, uintptr_t dst, void* src, size_t n) {
  struct apply_ctx* c = a; uint32_t addr = c->ws_lo + (uint32_t)dst;
  if (addr + n > c->ws_hi) return -DETOOLS_IO_FAILED;
  cwrite(addr, (const uint8_t*)src, n); return DETOOLS_OK;
}
static int dt_me(void* a, uintptr_t addr0, size_t n) {
  struct apply_ctx* c = a; uint32_t addr = c->ws_lo + (uint32_t)addr0;
  if (addr + n > c->ws_hi) return -DETOOLS_IO_FAILED;
  cerase(addr, n); return DETOOLS_OK;
}
static int dt_ss(void* a, int s) { ((struct apply_ctx*)a)->step = s; return DETOOLS_OK; }
static int dt_sg(void* a, int* s) { *s = ((struct apply_ctx*)a)->step; return DETOOLS_OK; }
static int dt_pr(void* a, uint8_t* dst, size_t n) {
  struct apply_ctx* c = a;
  if (c->patch_pos + n > c->patch_len) return -DETOOLS_IO_FAILED;
  fl_read(c->patch_addr + c->patch_pos, dst, n); c->patch_pos += (uint32_t)n; return DETOOLS_OK;
}

// ---- `.mota` parse (fixed fields only) + EndF base location --------------------------------------
struct mota_min {
  uint32_t total, image_size, payload_size, payload_addr, approval_addr;
  uint8_t  base_hash[8], image_hash[32], codec_id, is_full, approved;
};
static int parse_mota_at(uint32_t addr, struct mota_min* o) {
  uint8_t b[8 + MOTA_MFL];                          // MAGIC+total + the whole fixed manifest-minus-leaves
  uint32_t avail = MOTA_NRF52_FS_START - addr;
  uint32_t hdr = avail < sizeof(b) ? avail : sizeof(b);
  if (hdr < 8 + MOTA_MFL) return 0;                 // need the whole fixed manifest in `b` (trailer read separately)
  fl_read(addr, b, hdr);
  if (memcmp(b, MAGIC, 4) != 0) return 0;
  uint32_t total = rd_u32(b + 4);
  if (total < 8 + MOTA_MFL + 5 || total > avail) return 0;
  uint8_t tr[5]; fl_read(addr + total - 5, tr, 5);
  if (memcmp(tr, TRAILER, 5) != 0) return 0;

  // Fixed-layout manifest — every field at a constant offset; base_hash/signer/signature are always
  // present (zero-filled when not applicable), so there are no conditionals (docs/ota_protocol.md §4).
  br_t r = { b, hdr, 0, 1 };
  br_skip(&r, 4 + 4);                               // MAGIC + MOTA_TOTAL_SIZE (already validated above)
  if (br_u8(&r) != 2) return 0;                     // format_ver
  uint8_t flags  = br_u8(&r);
  br_u8(&r);                                        // hash_algo
  br_skip(&r, 4 + 4);                               // target_id, fw_version (unused here)
  o->image_size   = br_u32(&r);
  o->payload_size = br_u32(&r);
  uint8_t bsl     = br_u8(&r);
  br_skip(&r, 4);                                   // merkle_root
  const uint8_t* ih = br_take(&r, 32); if (ih) memcpy(o->image_hash, ih, 32);
  o->codec_id     = br_u8(&r);
  br_skip(&r, 32);                                  // hw_id (unused here)
  o->is_full      = (flags & MFLAG_FULL) ? 1 : 0;
  const uint8_t* bh = br_take(&r, 8); if (bh) memcpy(o->base_hash, bh, 8);   // base_hash (zero for full)
  br_skip(&r, 32 + 64);                             // signer pubkey + signature (zero when unsigned)
  if (!r.ok) return 0;
  o->approval_addr = addr + r.n;
  const uint8_t* ap = br_take(&r, 4);
  o->approved = (ap && memcmp(ap, APRV, 4) == 0) ? 1 : 0;
  if (bsl == 0 || bsl > 24 || o->payload_size == 0) return 0;
  uint32_t bs = 1u << bsl, bc = (o->payload_size + bs - 1) / bs;
  uint32_t off = r.n + bc * 4;                      // leaves[] then payload
  o->payload_addr = addr + off;
  o->total = total;
  if (off + o->payload_size + 5 != total) return 0; // payload must end exactly at the trailer
  return 1;
}

// Scan page boundaries for a valid `.mota`, returning the HIGHEST one (closest to FS_START).
//
// Direction matters for safety. The app stages the container bottom-aligned and writes [write_start,
// FS_START) contiguously (0xFF-padding the tail up to FS_START), so the *current* `.mota` is always the
// highest in flash; a leftover from a prior, differently-sized fetch sits strictly BELOW it (a larger
// new fetch overwrites everything from its lower start up to FS_START). Scanning top-down therefore
// returns the current container and never stops on a stale one — and the caller's APRV check is applied
// to THAT (highest) container only, so a stale lower `.mota` is never applied even if it is still
// approved. (EndF is the mirror image: the app image grows up from APP_BASE, so the current trailer is
// the LOWEST valid marker and find_body_len scans bottom-up. Each marker is scanned from the end where
// the current one is encountered first.)
#define MOTA_MIN_LEN  (8 + MOTA_MFL + 5)
static uint32_t scan_mota(struct mota_min* o) {
  uint32_t top = (MOTA_NRF52_FS_START - MOTA_MIN_LEN) & ~(PAGE - 1);
  for (uint32_t a = top + PAGE; a > APP_BASE; ) {        // walk page boundaries high -> low
    a -= PAGE;
    uint8_t m4[4]; fl_read(a, m4, 4);
    if (memcmp(m4, MAGIC, 4) == 0 && parse_mota_at(a, o)) return a;
  }
  return 0;
}

// Locate the running image's EndF trailer (= body_len) by scanning bottom-up for the self-validating
// marker (mirrors the app's FirmwareInfo::find_self_firmware). We deliberately do NOT trust
// bootloader_settings.bank_0_size: the UF2 flasher does not set it to the exact EndF-inclusive image
// size, so trusting it makes a freshly UF2-flashed app fail the base check and silently refuse its first
// OTA update. Bottom-up returns the CURRENT (lowest) trailer; a stale one from a prior larger image sits
// above it and is never reached. No hash check is needed here: the caller immediately recomputes
// sha256(body) and compares it to the delta's base_hash, so a (vanishingly unlikely) coincidental "EndF"
// just fails that gate and the update is refused — never misapplied. Byte-by-byte (like the app) so no
// body_len alignment is assumed; the scan stops at the first match (the current image's trailer).
static int find_body_len(uint32_t* body_len_out) {
  for (uint32_t off = 0; off + ENDF_LEN <= MOTA_NRF52_FS_START - APP_BASE; off++) {
    uint8_t e[8];
    fl_read(APP_BASE + off, e, 8);                  // marker(4) + body_len(4)
    if (memcmp(e, ENDF, 4) == 0 && rd_u32(e + 4) == off) { *body_len_out = off; return 1; }
  }
  return 0;
}

static void clear_approval(const struct mota_min* o) {
  uint8_t z[4] = { 0, 0, 0, 0 };
  cwrite(o->approval_addr, z, 4);
  cache_flush();
}

bool ota_delta_check_and_apply(void) {
  // Force a volatile read of the capability marker so -flto / --gc-sections cannot fold the reference away
  // and drop it — the running app scans the bootloader flash for it (ota_bl_info.h / OtaBlInfo.h).
  volatile uint8_t keep = *(const volatile uint8_t*)&g_mota_bl_info.magic[0];
  if (keep == 0) return false;                      // 'M' (0x4D) != 0, so never taken; keeps the marker live
  // ---- DIAGNOSTIC: stash a bail/progress code in GPREGRET2; the app reads it back into `ota status`.
  // 0xB0 entered (pre-gate) | 0xB1 gate passed (GPREGRET was 0x6A) | 0xB2 no/unapproved mota |
  // 0xB3 full/bad-codec | 0xB4 no body_len | 0xB5 base mismatch | 0x9N detools err N | 0xB6 wrong size |
  // 0xB7 result-hash mismatch | 0xB8 SUCCESS. If status shows 0xB0 -> GPREGRET wasn't 0x6A at the bootloader.
  gpregret2_set(0xB0);
  if (gpregret_get() != GPREGRET_OTA_APPLY) return false;
  gpregret_set(0);                                  // consume the trigger so we never loop
  gpregret2_set(0xB1);

  struct mota_min m;
  uint32_t mota_addr = scan_mota(&m);
  if (!mota_addr || !m.approved) { gpregret2_set(0xB2); return false; }   // nothing staged / not approved

  // base check (non-destructive): the running image's body must hash to the delta's base_hash.
  // Any pre-apply rejection clears the approval (this `.mota` is not applicable) and boots normally.
  uint32_t body_len;
  uint8_t base32[32];
  if (m.is_full || m.codec_id != CODEC_INPLACE) { gpregret2_set(0xB3); goto reject; }
  if (!find_body_len(&body_len))                { gpregret2_set(0xB4); goto reject; }
  sha256_region(APP_BASE, body_len, base32);
  if (memcmp(base32, m.base_hash, 8) != 0)      { gpregret2_set(0xB5); goto reject; }   // wrong base

  // commit point: clear approval BEFORE the destructive apply (a failure must not retry)
  clear_approval(&m);

  struct apply_ctx c;
  c.patch_addr = m.payload_addr; c.patch_len = m.payload_size; c.patch_pos = 0;
  c.ws_lo = APP_BASE; c.ws_hi = mota_addr; c.step = 0;          // workspace stays strictly below the mota
  int r = detools_apply_patch_in_place_callbacks(dt_mr, dt_mw, dt_me, dt_ss, dt_sg, dt_pr,
                                                 (size_t)m.payload_size, &c);
  cache_flush();
  if (r < 0) { gpregret2_set(0x90 | ((uint32_t)(-r) & 0x0F)); return false; }   // detools error code in low nibble
  if ((uint32_t)r != m.image_size) { gpregret2_set(0xB6); return false; }       // wrong reconstructed size

  uint8_t h[32];
  sha256_region(APP_BASE, m.image_size, h);
  if (memcmp(h, m.image_hash, 32) != 0) { gpregret2_set(0xB7); return false; }   // result mismatch -> DFU

  otah_settings_commit(BANK_VALID_APP_V, crc16_region(APP_BASE, m.image_size), m.image_size);
  gpregret2_set(0xB8);
  return true;

reject:
  clear_approval(&m);
  return false;
}

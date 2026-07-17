// Minimal FIPS-180 SHA-256 (public domain, based on Brad Conte's crypto-algorithms). The bootloader
// has no crypto library; the OTA delta-apply path needs SHA-256 to (a) recompute the running image's
// body hash (truncated to 8 bytes) for the base check and (b) verify the reconstructed image against
// the manifest's sha2-256:32. No malloc, no platform deps.
#ifndef OTA_SHA256_H_
#define OTA_SHA256_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t  data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} sha256_ctx_t;

void sha256_init(sha256_ctx_t* c);
void sha256_update(sha256_ctx_t* c, const uint8_t* data, size_t len);
void sha256_final(sha256_ctx_t* c, uint8_t out[32]);   // out = 32-byte digest

#ifdef __cplusplus
}
#endif

#endif // OTA_SHA256_H_

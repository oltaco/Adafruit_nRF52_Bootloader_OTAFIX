// MeshCore `.mota` delta-apply for the nRF52 bootloader (single-slot, in-place).
//
// Called once per boot, just before the app jump. If MeshCore staged + approved a delta and asked us
// to apply it (GPREGRET == GPREGRET_OTA_APPLY), we: scan flash for the `.mota`, confirm it is APPROVED
// and built for the *currently flashed* firmware (base_hash vs the running image's EndF body hash),
// clear the approval flag (so a failure never retries), apply the detools in-place patch over the app
// region, verify the result hashes to the manifest image_hash, and update the bootloader settings so
// the new image boots. On any failure after the (non-destructive) base check we leave the settings
// invalid → the bootloader falls through to DFU (UF2-recoverable); we never boot an unverified image.
#ifndef OTA_DELTA_H_
#define OTA_DELTA_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns true iff a pending update was applied + committed (caller should reset to boot it).
// Returns false if there was nothing to do, or it failed (caller continues the normal boot/DFU path).
bool ota_delta_check_and_apply(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_DELTA_H_

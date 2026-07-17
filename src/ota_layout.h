// Shared OTA flash-layout constants for the MeshCore `.mota` delta-apply path on nRF52840 (RAK4631).
// SINGLE SOURCE OF TRUTH — keep byte-identical with MeshCore src/helpers/ota/OtaFlashLayout_nrf52.h.
//
// The running app lives at the SoftDevice end (APP_BASE) up to the primary LittleFS region at
// FS_START. MeshCore stages a verified+approved `.mota` in the free flash below FS_START; on the next
// boot the bootloader scans [APP_BASE, FS_START) for it and applies it in place. APP_BASE here is the
// nominal S140 value; the bootloader itself uses the runtime DFU_BANK_0_REGION_START (== CODE_REGION_1
// _START) so it tracks the actual SoftDevice — the two must agree on a given device.

#ifndef OTA_LAYOUT_H_
#define OTA_LAYOUT_H_

#define MOTA_NRF52_APP_BASE    0x00026000u   // S140 end (== CODE_REGION_1_START on RAK4631)
// Scan/workspace ceiling: below the lowest FS region. Companion builds put ExtraFS at 0xD4000; the
// repeater leaves 0xD4000..0xED000 free. 0xD4000 is the safe universal ceiling for all RAK4631 roles.
#define MOTA_NRF52_FS_START    0x000D4000u
#define MOTA_NRF52_FLASH_PAGE  4096u

// GPREGRET value MeshCore writes (then resets) to ask the bootloader to apply a staged `.mota`.
// Distinct from the Adafruit DFU magics (0x57 UF2, 0x4E serial, 0xA8 OTA-BLE) so it never enters DFU.
#define GPREGRET_OTA_APPLY     0x6Au

#endif // OTA_LAYOUT_H_

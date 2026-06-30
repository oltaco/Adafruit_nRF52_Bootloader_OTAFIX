/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /** Flash start address for the bootloader. This setting will also be stored in UICR to allow the
   *  MBR to init the bootloader when starting the system. This value must correspond to 
   *  BOOTLOADER_REGION_START found in dfu_types.h. The system is prevented from starting up if 
   *  those values do not match. The check is performed in main.c, see
   *  APP_ERROR_CHECK_BOOL(*((uint32_t *)NRF_UICR_BOOT_START_ADDRESS) == BOOTLOADER_REGION_START);
   */
  /* MeshCore OTA: the in-place delta applier (detools CRLE in-place + sha256 + ota_delta) needs ~6 KB,
     and detools is already at its minimum here (only the CRLE+in-place paths survive --gc-sections; the
     code was slimmed as far as is safe). That pushes the bootloader ~0.2 KB past the stock 38 KB code
     region, so we reclaim 1 KB from the CF2 bootloaderConfig page and give it to FLASH -> 39 KB. That
     reclaim is safe: bootloaderConfig (boards/<board>/pinconfig.c) is a compile-time `const` array --
     read-only flash, NEVER written at runtime, so it cannot grow in the field. It is referenced by
     symbol in uf2/ghostfat.c (NOT a fixed address), currently ~88 B, and its CF2 format caps at 100
     entries (~816 B < 1 KB); a build that overran the 1 KB would fail at LINK time, not at runtime. This
     is migration-safe: the bootloader still starts at 0xF4000 (UICR/BOOTLOADER_REGION_START unchanged) and
     the MBR-params (0xFE000) and bootloader-settings (0xFF000) pages are untouched.

     Full nRF52840 (RAK4631) flash map — only the FLASH/CF2 boundary (*) moves vs stock:

         0x00000  MBR
         0x01000  SoftDevice (S140 6.1.1)
         0x26000  Application  (MeshCore; staged .mota lives just below the filesystem, < 0xD4000)
         ...
         0xF4000  ┌─ bootloader code (FLASH) ── 39 KB ──┐   stock: 38 KB, ends 0xFD800
         0xFDC00  ├─ CF2 bootloaderConfig ───── 1 KB ───┤ (*) stock: 2 KB, starts 0xFD800
         0xFE000  ├─ MBR params ─────────────── 4 KB ───┤   unchanged
         0xFF000  ├─ bootloader settings ────── 4 KB ───┤   unchanged
        0x100000  └─ (end of flash) ────────────────────┘
  */
     
  FLASH (rx) : ORIGIN = 0xF4000, LENGTH = 0xFE000-0xF4000 - 1K /* 39 KB */

  BOOTLOADER_CONFIG (r): ORIGIN = 0xFE000 - 1K, LENGTH = 1K

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0xFE000, LENGTH = 0x1000
  
  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0xFF000, LENGTH = 0x1000



  /** RAM Region for bootloader. */
  /* Avoid conflict with NOINIT for OTA bond sharing */
  RAM (rwx) :  ORIGIN = 0x20008000, LENGTH = 0x20040000-0x20008000

  /* Location for double reset detection, no init */
  DBL_RESET (rwx) :  ORIGIN = 0x20007F7C, LENGTH = 0x04
  
  /** Location of non initialized RAM. Non initialized RAM is used for exchanging bond information
   *  from application to bootloader when using buttonless DFU OTA. */
  NOINIT (rwx) :  ORIGIN = 0x20007F80, LENGTH = 0x80
  
  
  
  /** Location in UICR where bootloader start address is stored. */
  UICR_BOOTLOADER (r) : ORIGIN = 0x10001014, LENGTH = 0x04
  
  /** Location in UICR where mbr params page address is stored. */
  UICR_MBR_PARAM_PAGE(r) : ORIGIN = 0x10001018, LENGTH = 0x04
}

SECTIONS
{
  .fs_data_out ALIGN(4):
  {
    PROVIDE( __start_fs_data = .);
    KEEP(*(fs_data))
    PROVIDE( __stop_fs_data = .);
  } = 0

  .bootloaderConfig :
  {
    KEEP(*(.bootloaderConfig))
  } > BOOTLOADER_CONFIG

  /* Place the bootloader settings page in flash. */
  .bootloaderSettings(NOLOAD) :
  {
    
  } > BOOTLOADER_SETTINGS

  /* Write the bootloader address in UICR. */
  .uicrBootStartAddress : 
  {
    KEEP(*(.uicrBootStartAddress))
  } > UICR_BOOTLOADER
  
  /* Place the mbr params page in flash. */
    .mbrParamsPage(NOLOAD) :
  {
    
  } > MBR_PARAMS_PAGE
  
  /* Write the bootloader address in UICR. */
  .uicrMbrParamsPageAddress :
  {
    KEEP(*(.uicrMbrParamsPageAddress))
  } > UICR_MBR_PARAM_PAGE

  .dbl_reset(NOLOAD) :
  {

  } > DBL_RESET

  /* No init RAM section in bootloader. Used for bond information exchange. */
  .noinit(NOLOAD) :
  {

  } > NOINIT

  /* other placements follow here... */
}

INCLUDE "nrf_common.ld"

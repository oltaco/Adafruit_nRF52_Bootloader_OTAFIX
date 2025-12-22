# Adafruit nRF52 Bootloader with Enhanced OTA DFU

## Changes in OTAFIX 2.0

- **Defaults to OTA DFU mode**  
  When no valid application is present, the bootloader defaults to OTA DFU mode.  
  This prevents devices from becoming stuck in UF2 mode after a failed OTA update.

- **High-MTU BLE support**  
  Enables larger DFU packets for improved throughput when supported by the client.  
  The Android DFU app and [`dfu.py`](https://github.com/recrof/nrf_dfu_py) support large packets; the iOS DFU app is limited to 20-byte packets.

- **Lazy flash erase**  
  Flash pages are erased on demand during the transfer instead of upfront, significantly reducing the delay during DFU initialisation before the transfer begins.

- **Small-packet accumulation**  
  Packets smaller than 64 bytes are combined at the transport layer and written to flash in chunks of up to 240 bytes.  
  This improves OTA performance from iOS devices and other small-packet DFU hosts by reducing flash write overhead.

- **Automatic application boot after OTA over USB**  
  When connected to a USB host, devices now automatically reboot into the application after a successful OTA update, instead of requiring a manual reset.

- **Unique BLE advertising names per board**  
  In OTA DFU mode, devices advertise using a board-specific name instead of the generic `AdaDFU`:
  - **Heltec T114** → `T114_DFU`
  - **ProMicro NRF52840** → `PROM_DFU`
  - **T1000e** → `T1KE_DFU`
  - **WioTracker L1** → `WTL1_DFU`
  - **RAK 4631** → `4631_DFU`
  - **RAK WisMesh Tag** → `RTAG_DFU`
  - **XIAO NRF52 BLE / SENSE** → `XIAO_DFU`

---

## Boards supported

- Heltec Automation Mesh Node T114 / HT-nRF5262
- Nologo ProMicro NRF52840 (aka SuperMini NRF52840)
- Seeed Studio SenseCAP Card Tracker T1000-E
- Seeed Studio Wio Tracker L1
- Seeed Studio XIAO nRF52840 BLE (and Seeed SenseCAP Solar Node)
- Seeed Studio XIAO nRF52840 BLE SENSE
- RAK 4631 ([See note](#notes-on-RAK4631-bootloader))
- RAK WisMesh Tag (new 28/11/2025)

Any board already supported by the Adafruit nRF52 bootloader can be added.  
If there is another nRF52840-based board you are interested in, please raise an issue.

---

## Installation

**IMPORTANT:** If you are running a MeshCore companion firmware or Ripple firmware on your device **you will need to run an erase after flashing a new bootloader**. Use the MeshCore web flasher to do the erase, it will guide you to the correct erase firmware for your device. Other erase firmwares will not work, they will not erase the ExtraFS area.

The recommended way to install the bootloader is using the UF2 file.  
Download the UF2 file for your board (they can be found in the releases with filenames beginning with `update-`), enter UF2 mode (usually by double pressing the reset button within 0.5s) and copy the UF2 file across.

If you have somehow managed to accidentally flash an incorrect bootloader to your device you will likely require flashing a full bootloader and SoftDevice zip package using ``adafruit-nrfutil``

---

## Troubleshooting

### Device does not appear as a USB drive or serial port

If the device does not show up on your computer after flashing the bootloader or performing an OTA update, it may be **waiting in OTA DFU mode**.

In **OTAFIX 2.0**, OTA DFU is the default state when no valid application is present.  
In this mode:
- No UF2 drive is exposed
- No serial port is available
- The device is waiting for an OTA firmware update over BLE

**What to do:**
- Perform an OTA update using a supported DFU app, **or**
- Explicitly request UF2/serial mode using **double-reset**.

This behaviour is intentional and prevents devices from getting stuck in UF2 mode after failed OTA updates.

---

### OTA update fails with `Error: Operation Failed`

If an OTA update consistently fails early with `Error: Operation Failed`, this is often caused by BLE stack incompatibilities when **Request High MTU** is enabled.

**What to try:**
- Experiment with different PRN settings. Try 12, 8, 1, or even off altogether!
- Disable **Request High MTU** in your DFU app

While high MTU significantly improves performance on supported devices, it is not required for a successful OTA update.

---

## Recommended OTA DFU settings

To perform the OTA update you can use **nRF Device Firmware Update**  
([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.dfu&hl=en&gl=US) / [iOS](https://apps.apple.com/sa/app/device-firmware-update/id1624454660))  
or **nRF Connect**  
([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US) / [iOS](https://apps.apple.com/gb/app/nrf-connect-for-mobile/id1054362403)).

My preference is the **nRF Device Firmware Update** app.

For **OTAFIX 2.0**, the following settings are recommended (these may change — feel free to experiment and report your findings):

<table>
<tr>
<td valign="top">

**Packet Receipt Notification (PRN):** ON  
**Number of packets:** 30  
**Reboot time:** 0ms  
**Scan timeout:** 2000ms  
**Request high MTU:** ON for Android (See notes below) / Not available on iOS  
**Disable resume:** ON  
**Prepare object delay:** 0ms  
**Force scanning:** ON  
**Keep bond:** OFF  
**External MCU DFU:** OFF  

**Notes:**
- Some Android devices and BLE stacks do not behave well with **Request high MTU** enabled.  
  If the transfer fails early with `ERROR: Operation Failed`, retry with **Request high MTU turned OFF**.
- For maximum speed, Packet Receipt Notification can be disabled, and the number of packets increased.  
  Android is generally more tolerant of higher values; on iOS and other small-packet hosts, values above ~60 are not recommended.

</td>
</tr>
</table>

[Recommended settings for versions prior to 2.0 can be found here](docs/oldsettings.md).

**IMPORTANT:**  
On <u>older versions</u> of the bootloader, performing an OTA update while the device was connected to a computer USB host would complete successfully but **would not automatically boot into the new application firmware**, requiring a manual reset.  
This issue is fixed in **OTAFIX 2.0**.

---

## OTA update on a MeshCore repeater

First you will need to login to the repeater and issue the `start ota` CLI command.

Next, open the nRF Device Firmware Update app, select the appropriate MeshCore firmware zip file for your device, select your device (it will be advertised as `ProMicro_OTA` / `RAK4631_OTA`, etc), and press start.

---

## Donations

Although it's not necessary, if you find this useful please consider donating to support my work!

[![Ko-Fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/oltaco)

---

## Notes on RAK4631 bootloader

This version of the RAK4631 bootloader is based on a much newer version (0.9.2) of the Adafruit nRF52 bootloader than what RAK Wireless uses on their official bootloader (0.6.2-11).  

I haven't looked to see what changes (if any) that RAK made to the Adafruit bootloader, so I'm not sure if there's any difference but I have tested this bootloader and I haven't found any problems thus far. If you would rather use the original RAK bootloader but with these patches included you can find that [here](https://github.com/oltaco/WisCore_RAK4631_Bootloader/releases).
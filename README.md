
# Adafruit nRF52 Bootloader w/ Enhanced OTA DFU

#### Changes in OTAFIX 2.0
- **Defaults to OTA DFU mode**  
  When there is no valid app, default to OTA DFU mode.  
  Prevents devices from getting stuck in UF2 mode after a failed OTA update.

- **High-MTU BLE support**  
  Enables large packets for improved throughput when using an app which can support large packets. The Android DFU app and [dfu.py](https://github.com/recrof/nrf_dfu_py) both support large packets, the iOS DFU app does not.

- **Lazy flash erase**  
  Flash pages are erased on demand during the transfer instead of at the beginning before the transfer, significantly reducing the delay during DFU initialisation before the transfer starts.

- **Small-packet accumulation**  
  Packets smaller than 64 bytes are combined at the transport layer and written to flash in packets of up to 240 bytes. This improves OTA performance from iOS devices by reducing flash write overhead.

- **Automatic application boot after OTA over USB**  
  When connected to a USB host, devices now boot into the application after a successful OTA update instead of getting stuck and requiring a reset.

- **Unique BLE advertising name per-board**
  Instead of all board advertising as AdaDFU when in OTA DFU mode, they will advertise with a name unique to each board:  
  Heltec T114 - T114_DFU  
  ProMicro NRF52840 - PROM_DFU  
  T1000e - T1KE_DFU
  WioTrackerL1 - WTL1_DFU  
  RAK 4631 - 4631_DFU  
  Rak Wismesh Tag - RTAG_DFU  
  Xiao NRF52 BLE/SENSE - XIAO_DFU  

---
#### Boards supported:
- Heltec Automation Mesh Node T114 / HT-nRF5262
- Nologo ProMicro NRF52840 (aka SuperMini NRF52840)
- Seeed Studio SenseCAP Card Tracker T1000-E
- Seeed Studio Wio Tracker L1
- Seeed Studio XIAO nRF52840 BLE (and Seeed SenseCAP Solar Node)
- Seeed Studio XIAO nRF52840 BLE SENSE
- RAK 4631 ([See note](#notes-on-RAK4631-bootloader))
- RAK WisMesh Tag (new 28/11/2025)

Any board already supported by the Adafruit nrf52 bootloader can be added, or if there's another nRF52840-based board you're interested in please raise an issue.

---
#### Installation:
**IMPORTANT:** If you are running a MeshCore companion firmware or Ripple firmware on your device **you will need to run an erase after flashing a new bootloader**. Use the MeshCore web flasher to do the erase, it will guide you to the correct erase firmware for your device. Other erase firmwares will not work, they will not erase the ExtraFS area.

The recommended way to install the bootloader is using the UF2 file.
Download the UF2 file for your board (they can be found in the releases with filenames beginning with update-), enter UF2 mode (usually by double pressing the reset button within 0.5s) and copy the UF2 file across.

If you have somehow managed to accidentally flash an incorrect bootloader to your device you will likely require flashing a full bootloader and SoftDevice zip package using ``adafruit-nrfutil``  


---
#### Recommended settings and notes for doing OTA update:
To perform the OTA update you can use "nRF Device Firmware Update" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.dfu&hl=en&gl=US)/[iOS](https://apps.apple.com/sa/app/device-firmware-update/id1624454660)) or "nRF Connect" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US)/[iOS](https://apps.apple.com/gb/app/nrf-connect-for-mobile/id1054362403)). My preference is the "nRF Device Firmware Update" app.

For **OTAFIX-2.0** we recommend the following settings, but please note these may change! Feel free to experiment and let us know what you find.
<table>
<tr>
<td valign="top">

**Packets Receipt Notification:** ON  
**Number of packets:** 30  
**Reboot time:** 0ms  
**Scan timeout:** 2000ms  
**Request high MTU:** ON for Android / OFF for iOS  
**Disable resume:** ON  
**Prepare object delay:** 0ms  
**Force Scanning:** ON  
**Keep bond:** OFF  
**External MCU DFU:** OFF

Notes: You can turn off Packets Receipt Notification for fastest speed, and you can play with the number of packets. Android is more forgiving with numbers, iOS/small packets I don't recommend pushing above about 60.

</td>

</tr>
</table>

[The recommended settings for the versions before 2.0 can be found here](docs/oldsettings.md).


**IMPORTANT:** On <u>older versions</u> of the bootloader when you do an OTA update while your device is plugged into a computer the device will update but <U>it will not boot into the new application firmware</u>. It will require a manual reset in order to start. Plugged into a USB charger is fine. This is now fixed in v2.0

---
#### Performing an OTA update on a MeshCore repeater
First you will need to login to the repeater and issue the 'start ota' CLI command.

Next open the nRF Device Firmware Update app, select the appropriate MeshCore firmware zip file for your device, select your device (it will be called ProMicro_OTA/RAK4631_OTA etc), and press start.

----
#### Donations:
Although it's not necessary, if you find this useful please consider donating to support my work!

[![Ko-Fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/oltaco)

---
#### Notes on RAK4631 bootloader
This version of the RAK4631 bootloader is based on a much newer version (0.9.2) of the Adafruit nRF52 bootloader than what RAK Wireless uses on their official bootloader (0.6.2-11).  


I haven't looked to see what changes (if any) that RAK made to the Adafruit bootloader, so I'm not sure if there's any difference but I have tested this bootloader and I haven't found any problems thus far. If you would rather use the original RAK bootloader but with these patches included you can find that [here](https://github.com/oltaco/WisCore_RAK4631_Bootloader/releases).



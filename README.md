
# Adafruit nRF52 Bootloader w/ Enhanced OTA DFU

The purpose of this fork is to change the behaviour of the bootloader in OTA DFU mode. When the bootloader *detects* an invalid application firmware after an OTA update it will boot back to OTA DFU mode. This isn't totally bulletproof but if your device is difficult to access then this is a lot better than the default behaviour which is to boot into UF2/CDC mode on detection of an invalid application firmware. 

Additionally HCI_RX_BUF_QUEUE_SIZE is increased from 8 to 16. This is almost a necessity for OTA updates work on nRF52850 boards, otherwise a buffer overflow and crash in the bootloader is almost guaranteed. 


---
#### Boards:
- Nologo ProMicro NRF52840 (aka SuperMini NRF52840)
- Seeed Studio XIAO nRF52840 BLE
- Seeed Studio XIAO nRF52840 BLE SENSE
- RAK 4631 ([See note](#notes-on-RAK4631-bootloader))

Any board already supported by the Adafruit nrf52 bootloader can be added of course.

---
#### Installation:
The recommended way to install the bootloader is using the UF2 file.
Download the UF2 file for your board, enter UF2 mode (usually by double pressing the reset button within 0.5s) and copy the UF2 file across.

It's also possible to flash the zip or hex file but this is **not recommended** as on the offchance you get a bad flash you will need a JLink or SWD to recover.

---
#### Recommended settings and notes for doing OTA update:
To perform the OTA update you can use either "nRF Connect" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US)/[iOS](https://apps.apple.com/gb/app/nrf-connect-for-mobile/id1054362403)) or "nRF Device Firmware Update" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.dfu&hl=en&gl=US)/[iOS](https://apps.apple.com/sa/app/device-firmware-update/id1624454660)). My preference is the "nRF Device Firmware Update" app.

I use the following settings with the nRF Device Firmware Update app. Note that with the exception of "Force scanning" these are not set in stone.<br/><br/>

![DFU Settings Part 1/2](docs/dfu_settings_01.png) ![DFU Settings Part 2/2](docs/dfu_settings_02.png)


Notes on settings:
 - Force scanning (called Scan for bootloader in Legacy DFU in nRF Connect) is required, because when initiating OTA DFU mode the device will reboot and start advertising as AdaDFU with a different MAC address, Force scanning is what allows the app to find the device after it reboots.
 - Packets receipt notification is not strictly required but in my experience it makes the update less likely to fail, at the cost of being much slower.

**IMPORTANT:** If you do an OTA update while your device is plugged into a computer the device will update but <U>it will not boot into the new application firmware</u>. It will require a manual reset in order to start. Plugged into a USB charger is fine.

---
#### Performing an OTA update on a MeshCore repeater
First you will need to login to the repeater and issue the 'start ota' CLI command.

Next open the nRF Device Firmware Update app, select the appropriate MeshCore firmware zip file for your device, select your device (it will be called ProMicro_OTA/RAK4631_OTA etc), and press start.

Now cross your fingers and hope for the best!

---
#### Notes on RAK4631 bootloader
This version of the RAK4631 bootloader is based on a much newer version (0.9.2) of the Adafruit nRF52 bootloader than what RAK Wireless uses on their official bootloader (0.6.2-11).

I haven't looked to see what changes (if any) that RAK made to the Adafruit bootloader, so I'm not sure if there's any difference but I have tested this bootloader and I haven't found any problems thus far. If you would rather use the original RAK bootloader but with these patches included you can find that [here](https://github.com/oltaco/WisCore_RAK4631_Bootloader/releases).
\
\
\
\
\
\
\
\
\
\
\
\
\
\
\
\
\


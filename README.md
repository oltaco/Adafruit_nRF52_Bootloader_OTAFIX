
# Adafruit nRF52 Bootloader w/ Enhanced OTA DFU

The main of this fork is to change the behaviour of the bootloader when a DFU fails, along with some other patches that make it work on nRF52840. 
When this bootloader *detects* an invalid application firmware after a DFU update it will boot to OTA mode instead of UF2/CDC mode. This isn't totally bulletproof (sometimes the device will just hang at the end of DFU) but if your device is difficult to access then this is a lot better than the default behaviour.

---
#### Boards supported:
- Heltec Automation Mesh Node T114 / HT-nRF5262
- Nologo ProMicro NRF52840 (aka SuperMini NRF52840)
- Seeed Studio SenseCAP Card Tracker T1000-E
- Seeed Studio Wio Tracker L1
- Seeed Studio XIAO nRF52840 BLE
- Seeed Studio XIAO nRF52840 BLE SENSE
- RAK 4631 ([See note](#notes-on-RAK4631-bootloader))
- RAK WisMesh Tag (new 28/11/2025)

Any board already supported by the Adafruit nrf52 bootloader can be added, or if there's another nRF52840-based board you're interested in please raise an issue.

---
#### Installation:
**IMPORTANT:** If you are running a MeshCore companion firmware or Ripple firmware on your device **you will need to run an erase after flashing a new bootloader**. Use the MeshCore web flasher to do the erase, it will guide you to the correct erase firmware for your device. Other erase firmwares will not work, they will not erase the ExtraFS area.

The recommended way to install the bootloader is using the UF2 file.
Download the UF2 file for your board, enter UF2 mode (usually by double pressing the reset button within 0.5s) and copy the UF2 file across.

If you have somehow managed to accidentally flash an incorrect bootloader to your device you will likely require flashing a full bootloader and SoftDevice zip package using ``adafruit-nrfutil``  


---
#### Recommended settings and notes for doing OTA update:
To perform the OTA update you can use either "nRF Connect" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US)/[iOS](https://apps.apple.com/gb/app/nrf-connect-for-mobile/id1054362403)) or "nRF Device Firmware Update" ([Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.dfu&hl=en&gl=US)/[iOS](https://apps.apple.com/sa/app/device-firmware-update/id1624454660)). My preference is the "nRF Device Firmware Update" app on Android, but I have no experience with flashing from iOS devices.

We recommend the following settings for the DFU app.<br/><br/>

<table>
<tr>
<td valign="top">

**Packets Receipt Notification:** OFF  
**Number of packets:** 8  
**Reboot time:** 0ms  
**Scan timeout:** 2000ms  
**Request high MTU:** OFF  
**Disable resume:** ON  
**Prepare object delay:** 400ms  
**Force Scanning:** ON  
**Keep bond:** OFF  
**External MCU DFU:** OFF

</td>
<td valign="top">

Click the screenshot to view full.

<a href="docs/Screenshot_20251114_142334_DFU.jpg">
    <img src="docs/Screenshot_20251114_142334_DFU.jpg" width="200">
</a>

</td>
</tr>
</table>


**IMPORTANT:** If you do an OTA update while your device is plugged into a computer the device will update but <U>it will not boot into the new application firmware</u>. It will require a manual reset in order to start. Plugged into a USB charger is fine.

---
#### Performing an OTA update on a MeshCore repeater
First you will need to login to the repeater and issue the 'start ota' CLI command.

Next open the nRF Device Firmware Update app, select the appropriate MeshCore firmware zip file for your device, select your device (it will be called ProMicro_OTA/RAK4631_OTA etc), and press start.

Now cross your fingers and hope for the best!

----
#### Donations:
Although it's not necessary, if you find this useful please consider donating to support my work!

[![Ko-Fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/oltaco)

---
#### Notes on RAK4631 bootloader
This version of the RAK4631 bootloader is based on a much newer version (0.9.2) of the Adafruit nRF52 bootloader than what RAK Wireless uses on their official bootloader (0.6.2-11).  


I haven't looked to see what changes (if any) that RAK made to the Adafruit bootloader, so I'm not sure if there's any difference but I have tested this bootloader and I haven't found any problems thus far. If you would rather use the original RAK bootloader but with these patches included you can find that [here](https://github.com/oltaco/WisCore_RAK4631_Bootloader/releases).



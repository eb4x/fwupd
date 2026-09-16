---
title: Plugin: Genesys Logic
---

## Introduction

This plugin allows updating the Genesys Logic USB Hub devices.

* GL3521
* GL3523
* GL3525
* GL3590

Additionally, this plugin allows updating the MStar Semiconductor Scaler connected via an I²C bus.

* TSUM G
* MST9U

The MST9U is a level 1 part and reuses the `MSTAR_TSUM_G` instance ID prefix.

## Firmware Format

The daemon will decompress the cabinet archives and extract the firmware blob in an unspecified binary file format.

This plugin supports the following protocol IDs:

* `com.genesys.usbhub`
* `com.mstarsemi.scaler`

## GUID Generation

These devices use the standard USB DeviceInstanceId values for the USB Hub, e.g.

* `USB\VID_05E3&PID_0610` (quirk-only)

These devices use the standard USB DeviceInstanceId values for the HID under USB hub, e.g.

* `USB\VID_05E3&PID_0102` (quirk-only)

Additionally, some customized instance IDs are added. e.g.

* `USB\VID_03F0&PID_0610&IC_352330&BONDING_0F`
* `USB\VID_03F0&PID_0610&IC_352330&BONDING_0F&RUNMODE_M`
* `USB\VID_03F0&PID_0610&VENDOR_GENESYSLOGIC&IC_352330&BONDING_0F&PORTNUM_23&VENDORSUP_C09B5DD3-1A23-51D2-995A-F7366AAB3CA4`
* `USB\VID_05E3&PID_0630&PROJECT_1885D34D-0418-5EF8-8E69-4CEF77B6B6E8`
* `USB\VID_03F0&PID_0610&PUBKEY_AB859399-95B8-5817-B521-9AD8CC7F5BD6`

These devices also use custom GUID values for the Scaler, e.g.

* `GENESYS_SCALER\MSTAR_TSUM_G&PUBKEY_B335BDCE-7073-5D0E-9BD3-9B69C1A6899F&PANELREV_RIM101`
* `GENESYS_SCALER\VID_03F0&PID_0610&MSTAR_TSUM_G&PANELTYPE_EIM1`

The public key is product-specific. It identifies the product for scalers that report level 0.
Scalers that report level 1, for example the MStar MST9U, have no public key. Their firmware
file does not have a public key appended. The plugin does not verify a signature for either
level. For level 0, the plugin only compares the appended key with the key that it reads
from the scaler.

A level 1 scaler reports a *firmware ID* instead of a version, e.g. `EIM121`. Its first four
characters are the *panel type*, e.g. `EIM1`, and the last two change with each release. Only
the panel type survives an update, so that is what the GUID uses. To limit the match to one
product, the GUID also includes the vendor and product IDs of the hub. For level 0 scalers, the
public key does this.

The firmware ID is reported by the running firmware, not read from the panel. HP ships a
separate image for each panel variant, but the firmware ID in the image header is the same for
both variants of a release, so the parser does not use it to select an image.

## Quirk Use

This plugin uses the following plugin-specific quirks:

### `Flags=has-mstar-scaler`

USB Hub has a MStar Semiconductor Scaler attached via I²C.

Since 1.7.6.

### `Flags=has-public-key`

Device has a public-key appended to firmware.

Since 1.8.0

### `Flags=pause-r2-cpu`

Pause R2 CPU.

Since 1.7.6

### `Flags=replug-on-attach`

Leaving scaler ISP mode resets the scaler, and on some panels the USB hub re-enumerates too, so
wait for both devices to come back. The reset can happen before the hub has acknowledged the
request, so a failed transfer while leaving ISP mode is not an error on these panels.

Since 2.2.1

### `Flags=use-i2c-ch0`

Use I2C ch0.

Since 1.7.6

### GenesysUsbhubSwitchRequest

USB Hub Switch Request value.

* HP Mxfd FHD Monitors: `0xA1`

Since 1.7.6.

### GenesysUsbhubReadRequest

USB Hub Read Request value.

* HP Mxfd FHD Monitors: `0xA2`

Since 1.7.6.

### GenesysUsbhubWriteRequest

USB Hub Write Request value.

* HP Mxfd FHD Monitors: `0xA3`

Since 1.7.6.

### GenesysScalerDeviceTransferSize

Scaler Block size to use for transfers.

* MStar Semiconductor TSUM G: `0x40`

Since 1.7.6.

### GenesysScalerGpioOutputRegister

Scaler GPIO Output Register value.

* MStar Semiconductor TSUM G: `0x0426`

Since 1.7.6.

### GenesysScalerGpioEnableRegister

Scaler GPIO Enable Register value.

* MStar Semiconductor TSUM G: `0x0428`

Since 1.7.6.

### GenesysScalerGpioValue

Scaler GPIO value.

* MStar Semiconductor TSUM G: `0x01`

Since 1.7.6.

### GenesysScalerCfiFlashId

CFI Flash Id.

* HP M24fd USB-C Monitor: `0xC22016`
* HP M27fd USB-C Monitor: `0xC84016`
* HP Z27k G3 USB-C Monitor: `0xC22016`

The plugin does not read the flash ID from the scaler, because this requires ISP mode and blanks
the monitor. If this quirk is missing, the scaler is enumerated, but updates are inhibited.

Since 1.8.2.

## Firmware Bank Quirk Use

These parameters are used to configure and manage Genesys firmware banks.

This plugin uses the following plugin-specific quirks to indicate firmware bank detail:

### GenesysSupportDualBank

Indicates if dual-bank support is enabled.

Since 2.0.17

### GenesysSupportCodeSize

Indicatess if code size support is enabled.

Since 2.0.17

### GenesysHubBank1Address / GenesysHubBank2Address

Memory addresses for Hub bank 1 and 2.

Since 2.0.17

### GenesysHubBankCapacity

Capacity of the Hub bank.

Since 2.0.17

### GenesysDevBank1Address / GenesysDevBank2Address

Memory addresses for Device bank 1 and 2.

Since 2.0.17

### GenesysDevBankCapacity

Capacity of the Device bank.

Since 2.0.17

### GenesysPdBank1Address / GenesysPdBank2Address

Memory addresses for PD bank 1 and 2.

Since 2.0.17

### GenesysPdBankCapacity

Capacity of the PD bank.

Since 2.0.17

### GenesysCodesignBank1Address / GenesysCodesignBank2Address

Memory addresses for Codesign bank 1 and 2.

Since 2.0.17

### GenesysCodesignBankCapacity

Capacity of the Codesign bank.

Since 2.0.17

### GenesysFwDataMaxCount

Maximum count of firmware data.

Since 2.0.17

## Update Behavior

The devices are independently updated at runtime using USB control transfers.

The flash of an MST9U scaler holds two firmware images. Image A starts at 0x000000. Image B
starts at 0x200000. The boot loader is at the start of image A.

The boot loader selects the image with the newer version. It checks the CRC of that image.
It also checks the RSA signature, unless the board disables this check in hardware. If a
check fails, the boot loader starts the other image.

The plugin writes updates to image B only. The update does not change the boot loader or
image A. If image B is not valid, the boot loader starts image A.

Some boards disable the version, CRC and signature checks in hardware. On these boards, a
hardware register selects the image. The update can then have no effect. After an update,
read the firmware ID from the device to make sure that the update was applied.

The firmware is deployed when the device is in normal runtime mode, and the device will reset when the new firmware has been written.

The HP Mxfd FHD Monitors must be connected to host via the USB-C cable to apply an update. The devices remain functional during the update; the Scaler update is ~10 minute long.

## Vendor ID Security

The vendor ID is set from the USB vendor, for example set to `USB:0x03F0` for HP.

## External Interface Access

This plugin requires read/write access to `/dev/bus/usb`.

## Version Considerations

This plugin has been available since fwupd version `1.7.6`.

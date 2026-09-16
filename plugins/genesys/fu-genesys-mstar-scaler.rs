// Copyright 2026 Erik Berg <fwupd@slipsprogrammor.no>
// SPDX-License-Identifier: LGPL-2.1-or-later

// tail of the sBoot boot loader, at image offset 0x1FFE0
#[derive(New, ValidateStream, Default)]
#[repr(C, packed)]
struct FuStructGenesysMstarSboot {
    magic: [char; 10] == "MSVC0000S3",
    _reserved_0a: u8,
    _build_template: [char; 21],
}

// image info block, at image offset 0x20000
#[derive(New, ParseStream, Default)]
#[repr(C, packed)]
struct FuStructGenesysMstarInfo {
    _unknown_00: u32le,
    _unknown_04: u32le,
    _unknown_08: u32le,
    _unknown_0c: u32le,
    _reserved_10: u32le,
    _unknown_14: u32le,
    _reserved_18: [u8; 8],
    // the big-endian CRC-32 of [0, payload_length) is stored at payload_length
    payload_length: u32le,
    _payload_offset: u32le,
    _reserved_28: [u8; 60],
    // the flash offset of image B
    dual_image_offset: u32le,
    _reserved_68: [u8; 8],
    build_date: [char; 8],
    firmware_id: [char; 6],
    _reserved_7e: [u8; 2],
}

// updater trailer, the last 0x100 bytes of the file, XOR-obfuscated with the key "mstar"
#[derive(New, Parse, Default)]
#[repr(C, packed)]
struct FuStructGenesysMstarTrailer {
    magic: [char; 14] == "MTK_RSA_HEADER",
    _reserved_0e: [u8; 2],
    customer: [char; 24],
    chip: [char; 10],
    panel: [char; 24],
    odm: [char; 10],
    _firmware_id: [char; 10],
    _build_date: [char; 10],
    _reserved_68: [u8; 24],
    _flags: u32le,
    _reserved_84: u32le,
    dual_image_offset_hi: u8,
    _reserved_89: [u8; 119],
}

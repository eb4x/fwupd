/*
 * Copyright 2026 Erik Berg <fwupd@slipsprogrammor.no>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-genesys-mstar-scaler-firmware.h"

static void
fu_genesys_mstar_scaler_firmware_xml_func(void)
{
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autoptr(GError) error = NULL;

	/* write() builds a minimal image rather than re-emitting a parsed one */
	filename =
	    g_test_build_filename(G_TEST_DIST, "tests", "genesys-mstar-scaler.builder.xml", NULL);
	ret = fu_firmware_roundtrip_from_filename(filename,
						  "ed7f9c5ce490d36cc6bfca7474dffe00c41e160e",
						  FU_FIRMWARE_BUILDER_FLAG_NO_BINARY_COMPARE,
						  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
}

static void
fu_genesys_mstar_scaler_firmware_crc_func(void)
{
	gboolean ret;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *xml = NULL;
	g_autoptr(FuFirmware) firmware1 = NULL;
	g_autoptr(FuFirmware) firmware2 = g_object_new(FU_TYPE_GENESYS_MSTAR_SCALER_FIRMWARE, NULL);
	g_autoptr(FuFirmware) firmware3 = g_object_new(FU_TYPE_GENESYS_MSTAR_SCALER_FIRMWARE, NULL);
	g_autoptr(GByteArray) buf = g_byte_array_new();
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GBytes) blob_bad = NULL;
	g_autoptr(GError) error = NULL;

	filename =
	    g_test_build_filename(G_TEST_DIST, "tests", "genesys-mstar-scaler.builder.xml", NULL);
	ret = g_file_get_contents(filename, &xml, NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	firmware1 = fu_firmware_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(firmware1);
	blob = fu_firmware_write(firmware1, &error);
	g_assert_no_error(error);
	g_assert_nonnull(blob);

	/* the first byte is covered by the CRC, but by none of the other checks */
	fu_byte_array_append_bytes(buf, blob);
	buf->data[0] ^= 0xFF;
	blob_bad = g_bytes_new(buf->data, buf->len);

	ret = fu_firmware_parse_bytes(firmware2,
				      blob_bad,
				      0x0,
				      FU_FIRMWARE_PARSE_FLAG_NO_SEARCH,
				      &error);
	g_assert_error(error, FWUPD_ERROR, FWUPD_ERROR_INVALID_DATA);
	g_assert_false(ret);
	g_clear_error(&error);

	ret = fu_firmware_parse_bytes(firmware3,
				      blob_bad,
				      0x0,
				      FU_FIRMWARE_PARSE_FLAG_NO_SEARCH |
					  FU_FIRMWARE_PARSE_FLAG_IGNORE_CHECKSUM,
				      &error);
	g_assert_no_error(error);
	g_assert_true(ret);
}

int
main(int argc, char **argv)
{
	(void)g_setenv("G_TEST_SRCDIR", SRCDIR, FALSE);
	g_test_init(&argc, &argv, NULL);
	g_type_ensure(FU_TYPE_GENESYS_MSTAR_SCALER_FIRMWARE);
	g_test_add_func("/genesys/mstar-scaler-firmware/xml",
			fu_genesys_mstar_scaler_firmware_xml_func);
	g_test_add_func("/genesys/mstar-scaler-firmware/crc",
			fu_genesys_mstar_scaler_firmware_crc_func);
	return g_test_run();
}

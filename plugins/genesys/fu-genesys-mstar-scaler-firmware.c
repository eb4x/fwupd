/*
 * Copyright 2026 Erik Berg <fwupd@slipsprogrammor.no>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "fu-genesys-mstar-scaler-firmware.h"
#include "fu-genesys-mstar-scaler-struct.h"

/* sBoot occupies the first 128kB of the image and the build tool puts its marker in the last
 * 32 bytes of it, with the image info block immediately after */
#define FU_GENESYS_MSTAR_SCALER_SBOOT_OFFSET 0x1FFE0
#define FU_GENESYS_MSTAR_SCALER_INFO_OFFSET  0x20000

struct _FuGenesysMstarScalerFirmware {
	FuFirmware parent_instance;
	gchar *firmware_id;
	guint32 payload_length;
	guint32 dual_image_offset;
};

G_DEFINE_TYPE(FuGenesysMstarScalerFirmware, fu_genesys_mstar_scaler_firmware, FU_TYPE_FIRMWARE)

guint32
fu_genesys_mstar_scaler_firmware_get_dual_image_offset(FuGenesysMstarScalerFirmware *self)
{
	g_return_val_if_fail(FU_IS_GENESYS_MSTAR_SCALER_FIRMWARE(self), 0x0);
	return self->dual_image_offset;
}

static gboolean
fu_genesys_mstar_scaler_firmware_validate(FuFirmware *firmware,
					  FuInputStream *stream,
					  gsize offset,
					  GError **error)
{
	return fu_struct_genesys_mstar_sboot_validate_stream(
	    stream,
	    offset + FU_GENESYS_MSTAR_SCALER_SBOOT_OFFSET,
	    error);
}

/* the vendor updater obfuscates its trailer with a repeating ASCII key rather than encrypting it */
static void
fu_genesys_mstar_scaler_firmware_trailer_xor(GByteArray *buf)
{
	const gchar key[] = "mstar";

	for (guint i = 0; i < buf->len; i++)
		buf->data[i] ^= key[i % (sizeof(key) - 1)];
}

static FuStructGenesysMstarTrailer *
fu_genesys_mstar_scaler_firmware_parse_trailer(FuInputStream *stream, GError **error)
{
	gsize streamsz = 0;
	g_autoptr(GByteArray) buf = NULL;

	if (!fu_input_stream_size(stream, &streamsz, error))
		return NULL;
	if (streamsz < FU_STRUCT_GENESYS_MSTAR_TRAILER_SIZE) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_DATA,
				    "image is too small for an updater trailer");
		return NULL;
	}
	buf = fu_input_stream_read_byte_array(stream,
					      streamsz - FU_STRUCT_GENESYS_MSTAR_TRAILER_SIZE,
					      FU_STRUCT_GENESYS_MSTAR_TRAILER_SIZE,
					      NULL,
					      error);
	if (buf == NULL)
		return NULL;
	fu_genesys_mstar_scaler_firmware_trailer_xor(buf);
	return fu_struct_genesys_mstar_trailer_parse(buf->data, buf->len, 0x0, error);
}

static gboolean
fu_genesys_mstar_scaler_firmware_parse(FuFirmware *firmware,
				       FuInputStream *stream,
				       FuFirmwareParseFlags flags,
				       GError **error)
{
	FuGenesysMstarScalerFirmware *self = FU_GENESYS_MSTAR_SCALER_FIRMWARE(firmware);
	gsize streamsz = 0;
	guint32 dual_image_offset_trailer;
	g_autoptr(FuStructGenesysMstarInfo) st_info = NULL;
	g_autoptr(FuStructGenesysMstarTrailer) st_trailer = NULL;

	if (!fu_input_stream_size(stream, &streamsz, error))
		return FALSE;

	/* the sBoot marker has already been checked by ->validate() */
	st_info = fu_struct_genesys_mstar_info_parse_stream(stream,
							    FU_GENESYS_MSTAR_SCALER_INFO_OFFSET,
							    error);
	if (st_info == NULL) {
		g_prefix_error_literal(error, "failed to read info block: ");
		return FALSE;
	}
	self->dual_image_offset = fu_struct_genesys_mstar_info_get_dual_image_offset(st_info);
	if (self->dual_image_offset == 0) {
		g_set_error_literal(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_DATA,
				    "invalid dual-image offset of 0x0");
		return FALSE;
	}

	/* the plugin writes the file at the start of image B, so the file must not be larger than
	 * one image; a larger file is a dump of the full flash and cannot be installed */
	if (streamsz > self->dual_image_offset) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "file of 0x%x is larger than one image of 0x%x",
			    (guint)streamsz,
			    self->dual_image_offset);
		return FALSE;
	}

	st_trailer = fu_genesys_mstar_scaler_firmware_parse_trailer(stream, error);
	if (st_trailer == NULL) {
		g_prefix_error_literal(error, "failed to read updater trailer: ");
		return FALSE;
	}

	/* the info block and the trailer are written independently, so requiring them
	 * to agree rejects a file that merely starts like an MStar image */
	dual_image_offset_trailer =
	    (guint32)fu_struct_genesys_mstar_trailer_get_dual_image_offset_hi(st_trailer) << 16;
	if (self->dual_image_offset != dual_image_offset_trailer) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "invalid dual-image offset, got 0x%x and 0x%x",
			    self->dual_image_offset,
			    dual_image_offset_trailer);
		return FALSE;
	}

	/* the boot loader CRCs this much of the image, with the expected value appended to it */
	self->payload_length = fu_struct_genesys_mstar_info_get_payload_length(st_info);
	if (self->payload_length <= FU_GENESYS_MSTAR_SCALER_INFO_OFFSET ||
	    self->payload_length > streamsz - sizeof(guint32)) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_INVALID_DATA,
			    "invalid payload length 0x%x for image of 0x%x",
			    self->payload_length,
			    (guint)streamsz);
		return FALSE;
	}

	/* the boot loader checks the CRC of the image it selects and starts the other image if the
	 * check fails; an update with a bad CRC has no effect and reports no error */
	if ((flags & FU_FIRMWARE_PARSE_FLAG_IGNORE_CHECKSUM) == 0) {
		guint32 crc_actual = 0x0;
		guint32 crc_expected = 0;
		g_autoptr(FuInputStream) stream_crc = NULL;

		stream_crc = fu_partial_input_stream_new(stream, 0x0, self->payload_length, error);
		if (stream_crc == NULL)
			return FALSE;
		if (!fu_input_stream_compute_crc32(stream_crc,
						   FU_CRC_KIND_B32_POSIX,
						   &crc_actual,
						   error))
			return FALSE;
		/* sBoot uses a forward CRC-32, which is POSIX without its final inversion */
		crc_actual ^= G_MAXUINT32;
		if (!fu_input_stream_read_u32(stream,
					      self->payload_length,
					      &crc_expected,
					      G_BIG_ENDIAN,
					      error))
			return FALSE;
		if (crc_actual != crc_expected) {
			g_set_error(error,
				    FWUPD_ERROR,
				    FWUPD_ERROR_INVALID_DATA,
				    "CRC mismatch, got 0x%08x, expected 0x%08x",
				    crc_actual,
				    crc_expected);
			return FALSE;
		}
	}

	/* the ID in the image header reads the same for every panel variant of a release, so it
	 * is metadata only -- the per-variant panel type lives in the compressed payload */
	g_free(self->firmware_id);
	self->firmware_id = fu_struct_genesys_mstar_info_get_firmware_id(st_info);

	/* success */
	return TRUE;
}

static void
fu_genesys_mstar_scaler_firmware_export(FuFirmware *firmware,
					FuFirmwareExportFlags flags,
					XbBuilderNode *bn)
{
	FuGenesysMstarScalerFirmware *self = FU_GENESYS_MSTAR_SCALER_FIRMWARE(firmware);

	fu_xmlb_builder_insert_kv(bn, "firmware_id", self->firmware_id);
	fu_xmlb_builder_insert_kx(bn, "payload_length", self->payload_length);
	fu_xmlb_builder_insert_kx(bn, "dual_image_offset", self->dual_image_offset);
}

static gboolean
fu_genesys_mstar_scaler_firmware_build(FuFirmware *firmware, XbNode *n, GError **error)
{
	FuGenesysMstarScalerFirmware *self = FU_GENESYS_MSTAR_SCALER_FIRMWARE(firmware);
	const gchar *tmp;

	/* optional properties */
	tmp = xb_node_query_text(n, "firmware_id", NULL);
	if (tmp != NULL) {
		g_free(self->firmware_id);
		self->firmware_id = g_strdup(tmp);
	}

	/* success */
	return TRUE;
}

/* this builds a minimal image for the fuzzer corpus rather than re-emitting a parsed one, as some
 * info block fields are derived from the decompressed payload and cannot be recomputed */
static GByteArray *
fu_genesys_mstar_scaler_firmware_write(FuFirmware *firmware, GError **error)
{
	FuGenesysMstarScalerFirmware *self = FU_GENESYS_MSTAR_SCALER_FIRMWARE(firmware);
	guint32 crc;
	guint32 payload_length;
	g_autoptr(FuStructGenesysMstarInfo) st_info = fu_struct_genesys_mstar_info_new();
	g_autoptr(FuStructGenesysMstarSboot) st_sboot = fu_struct_genesys_mstar_sboot_new();
	g_autoptr(FuStructGenesysMstarTrailer) st_trailer = fu_struct_genesys_mstar_trailer_new();
	g_autoptr(GByteArray) buf = g_byte_array_new();
	g_autoptr(GBytes) blob = NULL;

	blob = fu_firmware_get_bytes_with_patches(firmware, error);
	if (blob == NULL)
		return NULL;
	payload_length = FU_GENESYS_MSTAR_SCALER_INFO_OFFSET + FU_STRUCT_GENESYS_MSTAR_INFO_SIZE +
			 g_bytes_get_size(blob);
	if (payload_length + sizeof(crc) + FU_STRUCT_GENESYS_MSTAR_TRAILER_SIZE >
	    self->dual_image_offset) {
		g_set_error(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_NOT_SUPPORTED,
			    "payload of 0x%x is larger than one image of 0x%x",
			    payload_length,
			    self->dual_image_offset);
		return NULL;
	}

	fu_byte_array_set_size(buf, FU_GENESYS_MSTAR_SCALER_SBOOT_OFFSET, 0x0);
	g_byte_array_append(buf, st_sboot->buf->data, st_sboot->buf->len);

	fu_struct_genesys_mstar_info_set_payload_length(st_info, payload_length);
	fu_struct_genesys_mstar_info_set_dual_image_offset(st_info, self->dual_image_offset);
	if (self->firmware_id != NULL &&
	    !fu_struct_genesys_mstar_info_set_firmware_id(st_info, self->firmware_id, error))
		return NULL;
	g_byte_array_append(buf, st_info->buf->data, st_info->buf->len);
	fu_byte_array_append_bytes(buf, blob);

	/* the boot loader reads the checksum from just past the payload */
	crc = fu_crc32(FU_CRC_KIND_B32_POSIX, buf->data, buf->len) ^ G_MAXUINT32;
	fu_byte_array_append_uint32(buf, crc, G_BIG_ENDIAN);

	fu_struct_genesys_mstar_trailer_set_dual_image_offset_hi(st_trailer,
								 self->dual_image_offset >> 16);
	fu_genesys_mstar_scaler_firmware_trailer_xor(st_trailer->buf);
	g_byte_array_append(buf, st_trailer->buf->data, st_trailer->buf->len);

	/* success */
	return g_steal_pointer(&buf);
}

static void
fu_genesys_mstar_scaler_firmware_finalize(GObject *object)
{
	FuGenesysMstarScalerFirmware *self = FU_GENESYS_MSTAR_SCALER_FIRMWARE(object);

	g_free(self->firmware_id);
	G_OBJECT_CLASS(fu_genesys_mstar_scaler_firmware_parent_class)->finalize(object);
}

static void
fu_genesys_mstar_scaler_firmware_init(FuGenesysMstarScalerFirmware *self)
{
	/* image B starts at 0x200000 in all known firmware files */
	self->dual_image_offset = 2 * FU_MB;
}

static void
fu_genesys_mstar_scaler_firmware_class_init(FuGenesysMstarScalerFirmwareClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	FuFirmwareClass *firmware_class = FU_FIRMWARE_CLASS(klass);
	object_class->finalize = fu_genesys_mstar_scaler_firmware_finalize;
	firmware_class->validate = fu_genesys_mstar_scaler_firmware_validate;
	firmware_class->parse = fu_genesys_mstar_scaler_firmware_parse;
	firmware_class->export = fu_genesys_mstar_scaler_firmware_export;
	firmware_class->build = fu_genesys_mstar_scaler_firmware_build;
	firmware_class->write = fu_genesys_mstar_scaler_firmware_write;
	fu_firmware_set_size_max(firmware_class, 16 * FU_MB);
}

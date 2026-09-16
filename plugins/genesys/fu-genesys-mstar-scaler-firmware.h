/*
 * Copyright 2026 Erik Berg <fwupd@slipsprogrammor.no>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <fwupdplugin.h>

#define FU_TYPE_GENESYS_MSTAR_SCALER_FIRMWARE (fu_genesys_mstar_scaler_firmware_get_type())
G_DECLARE_FINAL_TYPE(FuGenesysMstarScalerFirmware,
		     fu_genesys_mstar_scaler_firmware,
		     FU,
		     GENESYS_MSTAR_SCALER_FIRMWARE,
		     FuFirmware)

guint32
fu_genesys_mstar_scaler_firmware_get_dual_image_offset(FuGenesysMstarScalerFirmware *self)
    G_GNUC_NON_NULL(1);

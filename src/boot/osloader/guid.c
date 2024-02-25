/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <efi/spec.h>

/* efi/acpi.h */
EFI_GUID gEfiAcpiTableGuid = EFI_ACPI_TABLE_GUID;
EFI_GUID gEfiAcpi10TableGuid = ACPI_10_TABLE_GUID;
EFI_GUID gEfiAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;

/* efi/device_path.h */
EFI_GUID gEfiVirtualDiskGuid = EFI_VIRTUAL_DISK_GUID;
EFI_GUID gEfiVirtualCdGuid = EFI_VIRTUAL_CD_GUID;
EFI_GUID gEfiPersistentVirtualDiskGuid = EFI_PERSISTENT_VIRTUAL_DISK_GUID;
EFI_GUID gEfiPersistentVirtualCdGuid = EFI_PERSISTENT_VIRTUAL_CD_GUID;
EFI_GUID gEfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;

/* efi/edid_active.h */
EFI_GUID gEfiEdidActiveProtocolGuid = EFI_EDID_ACTIVE_PROTOCOL_GUID;

/* efi/file_info.h */
EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

/* efi/graphics_output.h */
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/* efi/loaded_image.h */
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiLoadedImageDevicePathProtocolGuid = EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

/* efi/rng.h */
EFI_GUID gEfiRngProtocolGuid = EFI_RNG_PROTOCOL_GUID;
EFI_GUID gEfiRngAlgorithmSp80090Hash256Guid = EFI_RNG_ALGORITHM_SP800_90_HASH_256_GUID;
EFI_GUID gEfiRngAlgorithmSp80090Hmac256Guid = EFI_RNG_ALGORITHM_SP800_90_HMAC_256_GUID;
EFI_GUID gEfiRngAlgorithmSp80090Ctr256Guid = EFI_RNG_ALGORITHM_SP800_90_CTR_256_GUID;
EFI_GUID gEfiRngAlgorithmX9313DesGuid = EFI_RNG_ALGORITHM_X9_31_3DES_GUID;
EFI_GUID gEfiRngAlgorithmX931AesGuid = EFI_RNG_ALGORITHM_X9_31_AES_GUID;
EFI_GUID gEfiRngAlgorithmRaw = EFI_RNG_ALGORITHM_RAW;
EFI_GUID gEfiRngAlgorithmArmRndr = EFI_RNG_ALGORITHM_ARM_RNDR;

/* efi/simple_file_system.h */
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

/* efi/simple_text_in.h */
EFI_GUID gEfiSimpleTextInProtocolGuid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;

/* efi/simple_text_in_ex.h */
EFI_GUID gEfiSimpleTextInputExProtocolGuid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;

/* efi/simple_text_out.h */
EFI_GUID gEfiSimpleTextOutProtocolGuid = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;

/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2026 Stefanos Stefanidis.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "config.h"
#include "serial.h"

BOOLEAN UseUefiConsole = FALSE;
BOOLEAN NoMenuLoad = FALSE;

static BOOLEAN ConfigFirstRun = FALSE;

static void
ConfigCrypto(UINT8 *SourceBuffer, UINT8 *TargetBuffer, UINTN Size)
{
    UINTN i;
    UINT32 Key = 0x73B5DBFA;

    for (i = 0; i < Size; i++) {
        TargetBuffer[i] = (SourceBuffer[i] ^ (Key & 0xFF));
        Key = (Key << 1) | (Key >> 31);
    }
}

/*
 * Initialize a decrypted default config file buffer to be encrypted later.
 */
static void
InitDefaultFile(UINT8 *Buffer, UINTN Size)
{
	UINTN i;

	for (i = 0; i < Size; i++)
		Buffer[i] = 0x00;
}

static EFI_STATUS
CheckSumConfig(struct ConfigFile *Cfg, BOOLEAN GenerateFlag)
{
    UINTN i;
    UINT16 TheSum = 0;
    UINT16 FileSum = 0;
    UINT8 *buf = (UINT8 *)Cfg;
    UINTN size = sizeof(*Cfg);
    UINTN chksum_off = size - sizeof(UINT16);

    if (size < sizeof(UINT16))
        return EFI_INVALID_PARAMETER;

    for (i = 0; i < size; i++) {
        if (i == chksum_off || i == chksum_off + 1)
            continue;
        TheSum = (UINT16)(TheSum + buf[i]);
    }

    if (GenerateFlag) {
        /* Store checksum in big-endian order (same on-disk format as before) */
        buf[chksum_off] = (UINT8)((TheSum >> 8) & 0xFF);
        buf[chksum_off + 1] = (UINT8)(TheSum & 0xFF);
        return EFI_SUCCESS;
    } else {
        FileSum = ((UINT16)buf[chksum_off] << 8) | (UINT16)buf[chksum_off + 1];
        if (FileSum == TheSum)
            return EFI_SUCCESS;
        else
            return EFI_CRC_ERROR;
    }
}

static EFI_STATUS
WriteDefaultConfig(UINT8 *Buffer, UINT8 *OutBuffer, EFI_FILE_HANDLE RootFS, EFI_FILE_HANDLE File, const CHAR16 *Path)
{
    EFI_STATUS Status;

    Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, Path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot create file %s: %r\n", Path, Status);
        return Status;
    }

    /*
     * Build a default ConfigFile in-memory, checksum it, encrypt and write.
     */
    struct ConfigFile Dec = { 0 };
    InitDefaultFile((UINT8 *)&Dec, sizeof(Dec));
    Dec.Magic = CONFIG_MAGIC;
    Dec.Version = CONFIG_FILE_VERSION;
    Dec.MenuFlag = FALSE;
    Dec.UefiConsoleFlag = FALSE;
    Dec.SerialPort = 0;
    Dec.SerialBaudRate = 115200;

    Status = CheckSumConfig(&Dec, TRUE);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    ConfigCrypto((UINT8 *)&Dec, OutBuffer, sizeof(Dec));

    UINTN WriteSize = sizeof(Dec);
    Status = uefi_call_wrapper(File->Write, 3, File, &WriteSize, OutBuffer);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot write to file %s: %r\n", Path, Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    uefi_call_wrapper(File->Close, 1, File);
    return EFI_SUCCESS;
}

/*
 * Function:
 * ReadConfig()
 *
 * Description:
 * Read a configuration file from a FAT volume.
 *
 * Arguments:
 * Path: File path.
 * OutCfg: optional pointer to a struct ConfigFile to receive the decoded config.
 *
 * Return value:
 * EFI_SUCCESS on sucess, any other code on failure.
 */
EFI_STATUS
ReadConfig(const UINT16 *Path, struct ConfigFile *OutCfg)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE File = NULL, RootFS = NULL;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs;
    UINTN ReadSize;
    UINT8 EncryptedBuf[sizeof(struct ConfigFile)];
    struct ConfigFile DecryptedCfg;
    UINT8 DefaultDec[sizeof(DecryptedCfg)];
    UINT8 DefaultEnc[sizeof(DefaultDec)];

    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot get loaded image protocol: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&SimpleFs);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot get Simple File System protocol: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(SimpleFs->OpenVolume, 2, SimpleFs, &RootFS);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot open filesystem volume: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
		if (Status == EFI_NOT_FOUND) {
            PrintToScreen(L"Config file not found, creating default.\n");
            Status = WriteDefaultConfig(DefaultDec, DefaultEnc, RootFS, File, Path);
            if (EFI_ERROR(Status)) {
                PrintToScreen(L"Failed to create default config file: %r\n", Status);
                return Status;
            }
        }

        PrintToScreen(L"Cannot open file %s: %r\n", Path, Status);
        return Status;
    }

    ReadSize = sizeof(EncryptedBuf);
    Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, EncryptedBuf);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot read file %s: %r\n", Path, Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    uefi_call_wrapper(File->Close, 1, File);

    /* Decrypt directly into the struct */
    ConfigCrypto(EncryptedBuf, (UINT8 *)&DecryptedCfg, ReadSize);

	Status = CheckSumConfig(&DecryptedCfg, FALSE);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Invalid config file checksum!\n");
        return Status;
    }

#if defined(DEBUG_BLD)
	PrintToScreen(L"No Menu Load flag:     0x%02x\n", DecryptedCfg.MenuFlag);
	PrintToScreen(L"Config file version:   0x%02x\n", DecryptedCfg.Version);
	PrintToScreen(L"Serial port:           0x%02x\n", DecryptedCfg.SerialPort);
	PrintToScreen(L"Serial port baud:      %u\n", DecryptedCfg.SerialBaudRate);
#endif

    /*
     * Read the settings from the decrypted buffer and set them accordingly.
     */
    if (!ConfigFirstRun) {
        NoMenuLoad = DecryptedCfg.MenuFlag ? TRUE : FALSE;
        UseUefiConsole = DecryptedCfg.UefiConsoleFlag ? TRUE : FALSE;
        SerialDownloadPort = DecryptedCfg.SerialPort;
        SerialBaud = DecryptedCfg.SerialBaudRate;
    }

    ConfigFirstRun = TRUE;

#if defined(DEBUG_BLD)
	PrintToScreen(L"SerialBaud: %u\n", SerialBaud);
#endif

    /*
     * Copy the decrypted to an output buffer if we did specify one.
     */
    if (OutCfg != NULL)
        *OutCfg = DecryptedCfg;

    return EFI_SUCCESS;
}

EFI_STATUS
WriteConfig(UINT8 Field, UINT32 Value)
{
     EFI_STATUS Status;
     EFI_FILE_HANDLE File = NULL, RootFS = NULL;
     EFI_LOADED_IMAGE *LoadedImage;
     EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs;
     UINTN ReadSize;
     UINT8 EncryptedBuffer[sizeof(struct ConfigFile)];
     struct ConfigFile DecryptedCfg;

     Status = uefi_call_wrapper(gBS->HandleProtocol, 3, gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
     if (EFI_ERROR(Status)) {
         PrintToScreen(L"Cannot get loaded image protocol: %r\n", Status);
         return Status;
     }

     Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&SimpleFs);
     if (EFI_ERROR(Status)) {
         PrintToScreen(L"Cannot get Simple File System protocol: %r\n", Status);
         return Status;
     }

     Status = uefi_call_wrapper(SimpleFs->OpenVolume, 2, SimpleFs, &RootFS);
     if (EFI_ERROR(Status)) {
         PrintToScreen(L"Cannot open filesystem volume: %r\n", Status);
         return Status;
     }

	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, CONFIG_FILE,
		EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
     if (EFI_ERROR(Status)) {
		if (Status == EFI_NOT_FOUND) {
			PrintToScreen(L"Config file not found!\n");
			return Status;
		}

		PrintToScreen(L"Failed to open config file: %r\n", Status);
		return Status;
	}

     ReadSize = sizeof(EncryptedBuffer);
     Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, EncryptedBuffer);
     if (EFI_ERROR(Status)) {
         PrintToScreen(L"Cannot read file %s: %r\n", CONFIG_FILE, Status);
         uefi_call_wrapper(File->Close, 1, File);
         return Status;
     }

     /* Rewind so we can overwrite the existing contents. */
     Status = uefi_call_wrapper(File->SetPosition, 2, File, 0);
     if (EFI_ERROR(Status)) {
         PrintToScreen(L"Failed to seek config file: %r\n", Status);
         uefi_call_wrapper(File->Close, 1, File);
         return Status;
     }
  
     // Decrypt buffer for writing.
     ConfigCrypto(EncryptedBuffer, (UINT8 *)&DecryptedCfg, ReadSize);

    switch (Field) {
        case CFG_FIELD_NOMENU:
            DecryptedCfg.MenuFlag = Value ? TRUE : FALSE;
            break;
        case CFG_FIELD_UEFI_CONSOLE:
            DecryptedCfg.UefiConsoleFlag = Value ? TRUE : FALSE;
            break;
        case CFG_FIELD_SERIAL_PORT:
            if (Value > 3) {
                PrintToScreen(L"Invalid serial port number. Valid numbers are 0-3.\n");
                return EFI_INVALID_PARAMETER;
            }
            DecryptedCfg.SerialPort = (UINT8)Value;
            break;
        case CFG_FIELD_SERIAL_BAUD:
            DecryptedCfg.SerialBaudRate = Value;
            break;
        case CFG_FIELD_CHKSUM:
        case CFG_FIELD_CHKSUM + 1:
        case CFG_FIELD_VERSION:
            PrintToScreen(L"Cannot write to reserved fields!\n");
            return EFI_UNSUPPORTED;
        default:
            PrintToScreen(L"Unknown config field: %d\n", Field);
            return EFI_INVALID_PARAMETER;
    }

    // Recalculate the checksum.
    Status = CheckSumConfig(&DecryptedCfg, TRUE);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    // Encrypt the buffer again for final write.
    ConfigCrypto((UINT8 *)&DecryptedCfg, EncryptedBuffer, ReadSize);

    /*
     * Only write back the bytes we actually read.
     */
    UINTN WriteSize = ReadSize;
    Status = uefi_call_wrapper(File->Write, 3, File, &WriteSize, EncryptedBuffer);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot write to file %s: %r\n", CONFIG_FILE, Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    uefi_call_wrapper(File->Close, 1, File);

    return EFI_SUCCESS;
}

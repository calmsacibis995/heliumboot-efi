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

BOOLEAN UseUefiConsole = FALSE;
BOOLEAN NoMenuLoad = FALSE;

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
CheckSumConfig(UINT8 *Buffer, UINTN Size, BOOLEAN GenerateFlag)
{
    UINTN i;
    UINT16 TheSum = 0;
    UINT16 FileSum = 0;

    /* Validate that the buffer is large enough to contain checksum entries */
    if (Size <= CONFIG_CHKSUM2_ENTRY)
        return EFI_INVALID_PARAMETER;

    /* Compute 16-bit sum of all bytes except the two checksum bytes */
    for (i = 0; i < Size; i++) {
        if (i == CONFIG_CHKSUM1_ENTRY || i == CONFIG_CHKSUM2_ENTRY)
            continue;
        TheSum = (UINT16)(TheSum + Buffer[i]);
    }

    if (GenerateFlag) {
        /* Store checksum (big endian: high byte first) */
        Buffer[CONFIG_CHKSUM1_ENTRY] = (UINT8)((TheSum >> 8) & 0xFF);
        Buffer[CONFIG_CHKSUM2_ENTRY] = (UINT8)(TheSum & 0xFF);
        return EFI_SUCCESS;
    } else {
        /* Read stored checksum and compare */
        FileSum = ((UINT16)Buffer[CONFIG_CHKSUM1_ENTRY] << 8) | (UINT16)Buffer[CONFIG_CHKSUM2_ENTRY];
        if (FileSum == TheSum)
            return EFI_SUCCESS;
        else
            return EFI_CRC_ERROR;
    }
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
 *
 * Return value:
 * EFI_SUCCESS on sucess, any other code on failure.
 */
EFI_STATUS
ReadConfig(const UINT16 *Path, UINT8 *OutBuf)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE File = NULL, RootFS = NULL;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs;
    UINTN ReadSize, i;
    UINT8 Buffer[256];
    UINT8 DecryptedBuffer[sizeof(Buffer)];
	UINT8 DefaultDec[256];
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

            Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, Path,
				EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
            if (EFI_ERROR(Status)) {
                PrintToScreen(L"Cannot create file %s: %r\n", Path, Status);
                return Status;
            }

            /*
			 * Create a minimal default decrypted config (256 bytes).
			 *
			 * Initialize the decrypted buffer, write the default values to it,
			 * encrypt it, and then write it to the file.
			 */
			InitDefaultFile(DefaultDec, sizeof(DefaultDec));

            DefaultDec[CONFIG_NOMENU_ENTRY] = FALSE;
			DefaultDec[CONFIG_VERSION_ENTRY] = CONFIG_FILE_VERSION;
            DefaultDec[CONFIG_UEFI_CONSOLE_ENTRY] = FALSE;
            
			CheckSumConfig(DefaultDec, sizeof(DefaultDec), TRUE);	// Generate checksum.

            ConfigCrypto(DefaultDec, DefaultEnc, sizeof(DefaultDec));

            UINTN WriteSize = sizeof(DefaultEnc);
            Status = uefi_call_wrapper(File->Write, 3, File, &WriteSize, DefaultEnc);
            if (EFI_ERROR(Status)) {
                PrintToScreen(L"Cannot write to file %s: %r\n", Path, Status);
                uefi_call_wrapper(File->Close, 1, File);
                return Status;
            }

            uefi_call_wrapper(File->Close, 1, File);
            return EFI_SUCCESS;
        }

        PrintToScreen(L"Cannot open file %s: %r\n", Path, Status);
        return Status;
    }

    ReadSize = sizeof(Buffer);
    Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Buffer);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Cannot read file %s: %r\n", Path, Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    uefi_call_wrapper(File->Close, 1, File);

    ConfigCrypto(Buffer, DecryptedBuffer, ReadSize);

	Status = CheckSumConfig(DecryptedBuffer, ReadSize, FALSE);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Invalid config file checksum!\n");
		return Status;
	}

#if defined(DEBUG_BLD)
	PrintToScreen(L"No Menu Load flag:     0x%02x\n", DecryptedBuffer[CONFIG_NOMENU_ENTRY]);
	PrintToScreen(L"Config file version:   0x%02x\n", DecryptedBuffer[CONFIG_VERSION_ENTRY]);
#endif

    /*
     * Read the settings from the decrypted buffer and set them accordingly.
     */
    if (DecryptedBuffer[CONFIG_NOMENU_ENTRY])
        NoMenuLoad = TRUE;
    else
        NoMenuLoad = FALSE;

    if (DecryptedBuffer[CONFIG_UEFI_CONSOLE_ENTRY])
        UseUefiConsole = TRUE;
    else
        UseUefiConsole = FALSE;

    /*
     * Copy the decrypted to an output buffer if we did specify one.
     */
    if (OutBuf != NULL) {
        for (i = 0; i < 256; i++)
            OutBuf[i] = DecryptedBuffer[i];
    }

    return EFI_SUCCESS;
}

EFI_STATUS
WriteConfig(UINT8 Field, UINT8 Value)
{
	EFI_STATUS Status;
    EFI_FILE_HANDLE File = NULL, RootFS = NULL;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs;
    UINTN ReadSize;
    UINT8 EncryptedBuffer[256];
    UINT8 DecryptedBuffer[sizeof(EncryptedBuffer)];

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
 
    ConfigCrypto(EncryptedBuffer, DecryptedBuffer, ReadSize);

    switch (Field) {
		case CONFIG_VERSION_ENTRY:
			PrintToScreen(L"Cannot write to version field!\n");
			return EFI_UNSUPPORTED;
		case CONFIG_CHKSUM1_ENTRY:
			PrintToScreen(L"Cannot write to checksum fields!\n");
			return EFI_UNSUPPORTED;
		case CONFIG_CHKSUM2_ENTRY:
			PrintToScreen(L"Cannot write to checksum fields!\n");
			return EFI_UNSUPPORTED;
		default:
			DecryptedBuffer[Field] = Value;
			break;
	}

	CheckSumConfig(DecryptedBuffer, ReadSize, TRUE);

	ConfigCrypto(DecryptedBuffer, EncryptedBuffer, ReadSize);

	/* Only write back the bytes we actually read. */
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

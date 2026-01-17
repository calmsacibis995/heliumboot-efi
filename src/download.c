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

/*
 * Portions of this code are derived from Symbian's ubootldr, adapted for UEFI,
 * which can be found here: https://github.com/cdaffara/symbiandump-os1/tree/master/sf/os/kernelhwsrv/brdbootldr/ubootldr
 *
 * These portions of code are licensed under the Eclipse Public License v1.0.
 * It is available at the URL "http://www.eclipse.org/legal/epl-v10.html".
 *
 * Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

BOOLEAN ImageZip = FALSE;
BOOLEAN ImageDeflated = FALSE;
EFI_FILE_PROTOCOL *BootFile;
UINTN FileSize = 0;
UINTN LoadSize = 0;
UINTN ImageReadProgress;

EFI_STATUS
DoZipDownload(EFI_FILE_PROTOCOL *BootFile)
{
    return EFI_SUCCESS;
}

EFI_STATUS
DoDeflateDownload(void)
{
    return EFI_SUCCESS;
}

/*
 * DoDownload() is ultimatly called by all the download routines, it is
 * responsible for unzipping images as well as writing disk images to drives.
 */
EFI_STATUS
DoDownload(void)
{
    EFI_STATUS Status;
    INTN BlockSize, Len;
    UINT8 *Dest;

    if (ImageZip) {
        PrintToScreen(L"Loading zip ..\n");
        Status = DoZipDownload(BootFile);
        if (EFI_ERROR(Status)) {
            PrintToScreen(L"\nFailed to download zip: %r\n", Status);
            return Status;
        }
    } else {
        LoadSize = FileSize;
        if (ImageDeflated) {
            PrintToScreen(L"\n\nLoading deflated image...\n");
            DoDeflateDownload();
            PrintToScreen(L"Deflated image download complete.\n");
        } else {
            if (FileSize == 0)
                BlockSize = 0x1000;
            else
                BlockSize = MAX(0x1000, FileSize >> 8);
            BlockSize = (BlockSize + 0xfff) &~ 0xfff;
            if (FileSize > 0)
                InitProgressBar(0, FileSize, "LOAD");
            Status = EFI_SUCCESS;

            while (Status == EFI_SUCCESS) {
                Len = BlockSize;
                Status = ReadInputData(Dest, Len);
                if (Status != EFI_SUCCESS && Status != EFI_END_OF_FILE)
                    break;
                Dest += Len;
                ImageReadProgress += Len;
                if (FileSize > 0)
                    UpdateProgressBar(0, ImageReadProgress);
            }
        }
    }

    return EFI_SUCCESS;
}

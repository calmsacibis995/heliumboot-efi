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

#ifndef _ZIP_H_
#define _ZIP_H_

#include <efi.h>
#include <efilib.h>

typedef enum {
    ZIP_HEADER_NOT_PROCESSED = 0,
    ZIP_HEADER_PROCESSING,
    ZIP_HEADER_DONE,
    ZIP_ERROR
} ZIP_HEADER_STATE;

struct ZipInfo {
    INTN Flags;
    INTN CompressionMethod;
    UINTN Crc;
    INTN CompressedSize;
    INTN UncompressedSize;
    INTN FilenameLength;
    INTN ExtraLength;
    INTN NameOffset;
    INTN DataOffset;
    CHAR8 *Filename;
    UINTN InBufSize;
    volatile UINTN FileBufW;
    volatile UINTN FileBufR;
    UINTN FileBufSize;
    UINT8 *FileBuf;
    ZIP_HEADER_STATE ProcessedHeader;
    volatile INTN HeaderDone;
    UINT8 *OutBuf;
    INTN Remain;
};

extern EFI_STATUS ReadBlockToBuffer(struct ZipInfo *Zip, EFI_FILE_PROTOCOL *BootFile);

#endif /* _ZIP_H_ */

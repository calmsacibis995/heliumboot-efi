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
 * Copyright (c) 1996-2009 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <efi.h>
#include <efilib.h>

#define KHEADER_BUFFER_SIZE 512
#define BIGG 'G'
#define BIGC 'C'

/* Packet/control bytes */
#define CAN 0x18
#define EOT 0x04
#define SOH 0x01
#define STX 0x02

struct YModem {
    INTN Timeout;
    INTN State;
    INTN BlockSize;       /* 128 or 1024 */
    INTN PacketSize;      /* BlockSize + 5 */
    UINT8 InitChar;
    UINT8 SeqNum;
    UINT16 Crc;
    INTN FileSize;
    UINT8 PacketBuf[1040];
    UINT8 FileName[256];
    BOOLEAN ImageDeflated;
    UINT8 HeaderBuf[KHEADER_BUFFER_SIZE];
};

extern UINT32 SerialDownloadPort, SerialBaud;

extern EFI_STATUS InitSerial(UINTN Port, UINTN Baud);
extern EFI_STATUS InitSerialDownload(UINTN Port);

#endif /* _SERIAL_H_ */

/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025, 2026 Stefanos Stefanidis.
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

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "serial.h"

EFI_SERIAL_IO_PROTOCOL *gSerial;
BOOLEAN LoadToFlash;
BOOLEAN FlashBootLoader;
BOOLEAN RomLoaderHeaderExists;
CHAR16 FileName[256];

UINT32 SerialDownloadPort;
UINT32 SerialBaud;

/* CRC table for CCITT */
static const UINT16 crcTab[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

/* Update CRC (converted from Symbian implementation) */
static void
UpdateCrc(struct YModem *y, const UINT8 *aPtr, INTN aLength)
{
    const UINT8 *pB = aPtr;
    UINT16 crc = y->Crc;
    while (aLength--)
        crc = (UINT16)((crc << 8) ^ crcTab[(crc >> 8) ^ *pB++]);
    y->Crc = crc;
}

/* Minimal ReadBlock wrapper over EFI serial read.
 * buf: buffer to fill
 * want: number of bytes desired to read (if 0, read whatever is available into 1 byte)
 * got_out: receives the number of bytes actually read
 *
 * Returns KErrNone or negative error.
 *
 * Note: This is a simple wrapper. For robust behavior implement proper timeouts and partial reads.
 */
static EFI_STATUS
ReadBlock(struct YModem *y, UINT8 *buf, UINTN want, UINTN *got_out)
{
    UINTN wanted = want > 0 ? want : 1;
    UINTN have = 0;
    UINTN readSize;
    EFI_STATUS Status;
    UINTN poll_us = 10000; /* 10ms poll interval */
    UINTN remaining_us = (y && y->Timeout) ? (UINTN)y->Timeout : 5000000; /* default 5s */

    /* If caller passed NULL buffer, read into a temporary buffer and discard */
    UINT8 tmp;
    if (buf == NULL) {
        buf = &tmp;
        wanted = 1;
    }

    while (have < wanted) {
        readSize = wanted - have;
        Status = uefi_call_wrapper(gSerial->Read, 2, gSerial, &readSize, buf + have);

        if (Status == EFI_SUCCESS) {
            /* accept any bytes read (partial read possible) */
            have += readSize;
            if (have >= wanted)
                break;
            /* if no bytes were returned, wait a little then retry */
            if (readSize == 0) {
                if (remaining_us <= poll_us)
                    break;
                uefi_call_wrapper(gBS->Stall, 1, poll_us);
                remaining_us -= poll_us;
            }
            continue;
        }

        if (Status == EFI_TIMEOUT) {
            /* timeout from the Serial driver */
            if (have > 0)
                break; /* return partial data to caller */
            /* else wait a short interval and retry until overall deadline */
            if (remaining_us <= poll_us)
                return EFI_TIMEOUT;
            uefi_call_wrapper(gBS->Stall, 1, poll_us);
            remaining_us -= poll_us;
            continue;
        }

        /* propagate other errors immediately */
        return Status;
    }

    if (got_out)
        *got_out = have;

    return (have == wanted) ? EFI_SUCCESS : EFI_TIMEOUT;
}

/* Read a single packet into y->PacketBuf (converted from YModem::ReadPacket) */
static EFI_STATUS
ReadPacket(struct YModem *y)
{
    EFI_STATUS Status;
    UINT8 *pD = y->PacketBuf;
    UINTN got;

    /* Read the first byte (packet control) */
    Status = ReadBlock(y, pD, 1, &got);
    if (EFI_ERROR(Status))
        return Status;

    if (got == 0)
        return EFI_TIMEOUT;

    UINT8 b0 = pD[0];
    if (b0 == CAN)
        return EFI_ABORTED;
    if (b0 == EOT)
        return EFI_END_OF_FILE;
    if (b0 == SOH)
        y->BlockSize = 128;
    else if (b0 == STX)
        y->BlockSize = 1024;
    else
        return EFI_PROTOCOL_ERROR;

    y->Timeout = 5000000;
    y->PacketSize = y->BlockSize + 5;

    /* Read rest of packet (packetSize - 1 bytes) */
    Status = ReadBlock(y, pD + 1, (UINTN)(y->PacketSize - 1), &got);
    if (Status != EFI_SUCCESS && Status != EFI_TIMEOUT)
        return Status;

    if (got < (UINTN)(y->PacketSize - 1))
        return EFI_BUFFER_TOO_SMALL;

    UINT8 seq = pD[1];
    UINT8 seqbar = pD[2];
    seqbar ^= seq;
    if (seqbar != 0xFF)
        return EFI_PROTOCOL_ERROR;

    if (seq == y->SeqNum)
        return EFI_ALREADY_STARTED;
    else {
        UINT8 nextseq = (UINT8)(y->SeqNum + 1);
        if (seq != nextseq)
            return EFI_PROTOCOL_ERROR;
    }

    y->Crc = 0;
    UpdateCrc(y, pD + 3, y->BlockSize);
    /* Validate CRC appended (big-endian in packet) */
    UINT16 rx_crc = (UINT16)((pD[y->PacketSize - 2] << 8) | pD[y->PacketSize - 1]);
    if (rx_crc != y->Crc)
        return EFI_CRC_ERROR;

    ++y->SeqNum;
    return EFI_SUCCESS;
}

static EFI_STATUS
ReadYModemInputData(UINT8 *Dest, UINTN *Length)
{
    // not implemented
    return EFI_UNSUPPORTED;
}

/* Parse packet 0 or copy data packet payload (converted from YModem::CheckPacket) */
static EFI_STATUS
CheckPacket(struct YModem *y, UINT8 *aDest)
{
    const UINT8 *p = y->PacketBuf + 3;
    if (y->State > 0) {
        if (aDest)
            MemCopy(aDest, p, y->BlockSize);
    } else {
        /* parse packet 0 (file name & size) */
        const char *nameptr = (const char *)p;
        size_t nLen = StrnLen((CHAR16 *)nameptr, y->BlockSize);
        if (nLen == 0)
            return EFI_END_OF_FILE; /* batch termination packet */

        /* copy filename (truncate to fit) */
        size_t copyLen = (nLen < sizeof(y->FileName) - 1) ? nLen : (sizeof(y->FileName) - 1);
        MemCopy(y->FileName, nameptr, copyLen);
        y->FileName[copyLen] = '\0';

        p += nLen + 1;
        y->FileSize = 0;
        for (;;) {
            UINT8 c = *p++;
            if (c < '0' || c > '9')
                break;
            y->FileSize *= 10;
            y->FileSize += c - '0';
        }
    }
    return EFI_SUCCESS;
}

/* Replace the previous stub with a minimal driver that uses ReadPacket/CheckPacket.
 * Returns:
 *  KErrNone (0) when the initial description packet was processed,
 *  positive N (bytes copied) for actual data packets,
 *  negative on error.
 */
static EFI_STATUS
ReadPackets(struct YModem *y, UINT8 *outBuf, UINTN maxLen)
{
    EFI_STATUS Status;

    /* If an earlier transfer left the ROM image deflated, drain remaining data */
    if (y->ImageDeflated && outBuf != NULL) {
        Status = ReadPacket(y);
        if (EFI_ERROR(Status))
            return Status;
        /* attempt to copy remainder into outBuf */
        INTN copy = (INTN)MIN((UINTN)y->BlockSize, maxLen);
        MemCopy(outBuf, y->PacketBuf + 3, copy);
        return copy;
    }

    /* Read one packet and process it */
    Status = ReadPacket(y);
    if (EFI_ERROR(Status))
        return Status;

    Status = CheckPacket(y, outBuf);
    if (EFI_ERROR(Status))
        return Status;

    /* If it was the description (packet 0) return success (0) */
    if (y->State == 0) {
        /* packet 0 processed */
        return EFI_SUCCESS;
    }

    /* otherwise it's a data packet: return number of bytes available */
    return (INTN)MIN((UINTN)y->BlockSize, maxLen);
}

EFI_STATUS
InitSerial(UINTN Port, UINTN Baud)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles;
    UINTN Count;

    Status = uefi_call_wrapper(gBS->LocateHandleBuffer, 5, ByProtocol, &gEfiSerialIoProtocolGuid, NULL, &Count, &Handles);
    if (EFI_ERROR(Status) || Port >= Count)
        return EFI_NOT_FOUND;

    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, Handles[Port], &gEfiSerialIoProtocolGuid, (VOID **)&gSerial);
    if (EFI_ERROR(Status))
        return Status;

    Status = uefi_call_wrapper(gSerial->SetAttributes, 7, gSerial, Baud, 0, 0, DefaultParity, 8, DefaultStopBits);
    if (EFI_ERROR(Status))
        return Status;

    return EFI_SUCCESS;
}

/* Convert of Symbian TInt YModem::StartDownload(TBool aG, TInt& aLength, TDes& aName)
 * to a UEFI-friendly function that returns EFI_STATUS and outputs size/name.
 */
static EFI_STATUS
StartYModemDownload(UINTN Port, UINTN *OutFileSize, CHAR16 *OutName, UINTN OutNameSize)
{
    struct YModem ym = { 0 };
    EFI_STATUS Status;

    /* If an earlier transfer left the ROM image deflated, drain remaining data */
    if (ym.ImageDeflated) {
        UINT8 *pD = ym.HeaderBuf;
        Status = ReadPackets(&ym, pD, KHEADER_BUFFER_SIZE);
        PrintToScreen(L"Remaining data read: %d, r:%u (%r)\n", (int)(pD - ym.HeaderBuf), Status, Status);
        if (EFI_ERROR(Status)) {
            PrintToScreen(L"Failed to read packets: %r\n", Status);
            return Status;
        }
    }

    /* Default to G-mode (as original used aG parameter). Adjust if needed. */
    ym.InitChar = (UINT8)BIGG;
    ym.State = 0;
    ym.SeqNum = 255;

    /* Read the file description packet. In Symbian code this was ReadPackets(NULL,0). */
    Status = ReadPackets(&ym, NULL, 0);
    if (!EFI_ERROR(Status)) {
        /* success: provide file size and convert ASCII filename to CHAR16 */
        if (OutFileSize)
            *OutFileSize = (UINTN)ym.FileSize;

        if (OutName && OutNameSize > 0) {
            UINTN i;
            for (i = 0; i + 1 < OutNameSize && ym.FileName[i] != '\0'; i++)
                OutName[i] = (CHAR16)ym.FileName[i];
            OutName[i] = L'\0';
        }
        return EFI_SUCCESS;
    } else
        return Status;

    return EFI_DEVICE_ERROR;
}

EFI_STATUS
InitSerialDownload(UINTN Port)
{
    EFI_STATUS Status;
    UINTN FileSize = 0;
    CHAR16 Name[ARRAY_SIZE(FileName)];

    // Use the configured baud rate if set, otherwise default to 115200.
    UINTN Baud = (SerialBaud != 0) ? (UINTN)SerialBaud : 115200;

    Status = InitSerial(Port, Baud);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"InitSerialDownload: cannot init serial port %u: %r\n", Port, Status);
        return Status;
    }

    SerialDownloadPort = (UINT32)Port;

    Status = StartYModemDownload(Port, &FileSize, Name, ARRAY_SIZE(Name));
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"YModem download failed: %r\n", Status);
        return Status;
    }

    // Detect ZIP by filename suffix.
    if (StrLen(Name) >= 4 && StrnCmp(Name + StrLen(Name) - 4, L".zip", 4) == 0 && FileSize != 0)
        ImageZip = TRUE;
    else
        ImageZip = FALSE;

    // Detect flash images by filename prefix "FLASHIMG" or "FLASHLDR"
    if (StrLen(Name) >= 8 &&
        (StrnCmp(Name, L"FLASHIMG", 8) == 0 || StrnCmp(Name, L"FLASHLDR", 8) == 0) &&
        FileSize != 0) {
        LoadToFlash = TRUE;
        if (StrnCmp(Name, L"FLASHLDR", 8) == 0)
            FlashBootLoader = TRUE;
    } else {
        LoadToFlash = FALSE;
        FlashBootLoader = FALSE;
    }

    // If not zip, determine inner compression (implement GetInnerCompression to analyze data buffer)
    if (!ImageZip) {
        Status = GetInnerCompression();
        if (EFI_ERROR(Status)) {
            PrintToScreen(L"Cannot get inner compression: %r\n", Status);
            return Status;
        }
    }

    // Populate FileName global
    StrnCpy(FileName, Name, ARRAY_SIZE(FileName) - 1);
    FileName[ARRAY_SIZE(FileName) - 1] = L'\0';

    InputFunction = ReadYModemInputData;

    return EFI_SUCCESS;
}

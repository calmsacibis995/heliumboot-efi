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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <assert.h>

struct ConfigFile {
    UINT16 Magic;
    UINT8 Version;
    BOOLEAN MenuFlag;
    BOOLEAN UefiConsoleFlag;
    UINT8 SerialPort;
    UINT32 SerialBaudRate;
    UINT8 Padding[244];
    UINT16 CheckSum;
} __attribute__((packed));

static_assert(sizeof(struct ConfigFile) == 256);

#define CONFIG_FILE_VERSION		3
#define CONFIG_FILE             L"config.dat"
#define CONFIG_MAGIC            0xA345

#define CFG_FIELD_MAGIC         0
#define CFG_FIELD_VERSION       2
#define CFG_FIELD_NOMENU        3
#define CFG_FIELD_UEFI_CONSOLE  4
#define CFG_FIELD_SERIAL_PORT   5
#define CFG_FIELD_SERIAL_BAUD   6
#define CFG_FIELD_CHKSUM        254

extern BOOLEAN NoMenuLoad;
extern BOOLEAN UseUefiConsole;
extern EFI_STATUS ReadConfig(const UINT16 *Path, struct ConfigFile *OutCfg);
extern EFI_STATUS WriteConfig(UINT8 Field, UINT32 Value);

#endif /* _CONFIG_H_ */

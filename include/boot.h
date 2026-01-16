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

#ifndef _BOOT_H_
#define _BOOT_H_

#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b)   ((a) < (b) ? (a) : (b))

// Global functions and variables, sorted by filename.

// boot.c
extern BOOLEAN exit_flag;
extern EFI_HANDLE gImageHandle;

// config.c
extern BOOLEAN NoMenuLoad;
extern EFI_STATUS ReadConfig(const UINT16 *Path);
extern EFI_STATUS WriteConfig(UINT8 Field, UINT8 Value);

// exec_efi.c
extern BOOLEAN IsEfiBinary(const void *Buffer);
extern EFI_STATUS LoadEfiBinary(CHAR16 *Path, EFI_LOADED_IMAGE *LoadedImage, EFI_HANDLE DeviceHandle, CHAR16 *ProgArgs);

// exec_elf.c
extern BOOLEAN IsElf64(UINT8 *Header);

// helpers.c
extern UINTN StrDecimalToUintn(CHAR16 *str);
extern void SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments);
extern void HeliumBootPanic(EFI_STATUS Status, const CHAR16 *fmt, ...);
extern UINT32 SwapBytes32(UINT32 val);
extern UINT16 SwapBytes16(UINT16 val);
extern UINT64 GetTimeSeconds(void);
extern void *MemMove(void *dst, const void *src, UINTN len);
extern void *MemCopy(void *Dest, const void *Src, UINTN Length);
extern CHAR16 *GetScreenInfo(void);
#if defined(X86_64_BLD)
extern void AsmCpuid(UINT32 Leaf, UINT32 subleaf, UINT32 *Eax, UINT32 *Ebx, UINT32 *Ecx, UINT32 *Edx);
#endif
extern UINT64 GetTotalMemoryBytes(void);
extern CHAR16 *StrStr(const CHAR16 *haystack, const CHAR16 *needle);

// loadfile.c
extern EFI_STATUS LoadFile(CHAR16 *args);
extern EFI_STATUS LoadElfBinary(EFI_FILE_HANDLE File);

// video.c
extern EFI_STATUS InitVideo(void);
extern void ClearScreen(void);
extern void SetPos(INTN X, INTN Y);
extern void PrintToScreen(const CHAR16 *Fmt, ...);
extern void InputToScreen(CHAR16 *Prompt, CHAR16 *InStr, UINTN StrLen);
extern void InitProgressBar(INTN Id, UINTN BarLimit, CONST CHAR8 *Buffer);
extern void UpdateProgressBar(INTN Id, UINTN NewProgress);
extern void GetScreenSize(UINTN *ScreenWidth, UINTN *ScreenHeight);
extern BOOLEAN VideoInitFlag;
extern BOOLEAN FramebufferAllowed;

// vers.c
extern const CHAR16 *getdate(void);
extern const CHAR16 *getvernum(void);
extern const CHAR16 *getversion(void);
extern const CHAR16 *getrevision(void);
extern const CHAR16 *getblddate(void);
extern const CHAR16 *getrevdate(void);
extern const CHAR16 *getcommitno(void);
extern const CHAR16 *getbuildno(void);

#endif /* _BOOT_H_ */

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
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

// Global typedef's and structs.

typedef EFI_STATUS (*InputFunc)(UINT8 *Dest, UINTN *Length);

// Global functions and variables, sorted by filename.

// download.c
extern BOOLEAN ImageDeflated;
extern BOOLEAN ImageZip;
extern UINTN FileSize, LoadSize;
extern EFI_STATUS DoDownload(void);

// exec_efi.c
extern BOOLEAN IsEfiBinary(const void *Buffer);
extern EFI_STATUS LoadEfiBinary(CHAR16 *Path, EFI_LOADED_IMAGE *LoadedImage, EFI_HANDLE DeviceHandle, CHAR16 *ProgArgs);

// exec_elf.c
extern BOOLEAN IsElf64(UINT8 *Header);
extern EFI_STATUS LoadElfBinary(EFI_HANDLE ImageHandle, EFI_FILE_HANDLE File);

// helpers.c
extern InputFunc InputFunction;
extern UINTN StrDecimalToUintn(CHAR16 *str);
extern void SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments);
extern void HeliumBootPanic(EFI_STATUS Status, const CHAR16 *fmt, ...);
extern UINT32 SwapBytes32(UINT32 val);
extern UINT16 SwapBytes16(UINT16 val);
extern UINT64 GetTimeSeconds(void);
extern INTN MemCmp(const void *p1, const void *p2, UINTN Size);
extern void *MemMove(void *dst, const void *src, UINTN len);
extern void *MemCopy(void *Dest, const void *Src, UINTN Length);
extern void *MemSet(void *dst, UINT8 value, UINTN size);
extern CHAR16 *GetScreenInfo(void);
#if defined(X86_64_BLD)
extern void AsmCpuid(UINT32 Leaf, UINT32 subleaf, UINT32 *Eax, UINT32 *Ebx, UINT32 *Ecx, UINT32 *Edx);
#endif
extern UINT64 GetTotalMemoryBytes(void);
extern CHAR16 *StrStr(const CHAR16 *haystack, const CHAR16 *needle);
extern void HexDump(UINT8 *Address, UINTN Length);
extern EFI_STATUS ReadInputData(UINT8 *Dest, UINTN *Length);
extern EFI_STATUS ReadAndPrintChar(EFI_SERIAL_IO_PROTOCOL *Serial);
extern UINT8 *DestinationAddress(void);

// loadfile.c
extern EFI_STATUS LoadFile(CHAR16 *args);

// main.c
#if _LP64
extern UINT64 *ActualDestinationAddress;
#else
extern UINT32 *ActualDestinationAddress;
#endif
extern BOOLEAN exit_flag;
extern EFI_HANDLE gImageHandle;

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

// zip.c
extern UINTN ImageReadProgress;

#endif /* _BOOT_H_ */

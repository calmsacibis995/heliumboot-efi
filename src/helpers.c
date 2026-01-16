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

#include <efi.h>
#include <efilib.h>
#include <stdarg.h>

#include "boot.h"
#include "part.h"

void
SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments)
{
	CHAR16 *p;

	*command = line;
	*arguments = L"";

	p = line;
	while (*p != L'\0' && *p != L' ')
		p++;

	if (*p == L' ') {
		*p = L'\0';
		p++;

		while (*p == L' ')
			p++;

		*arguments = p;
	}
}

UINTN
StrDecimalToUintn(CHAR16 *str)
{
    UINTN result = 0;

    while (*str >= L'0' && *str <= L'9') {
        result = result * 10 + (*str - L'0');
        str++;
    }

    return result;
}

/*
 * Function:
 * HeliumBootPanic()
 *
 * Description:
 * The equivalent of panic() in Unix and its clones. It is called on unresolvable
 * fatal errors.
 *
 * Arguments:
 * Status: EFI status code
 * fmt: Formatted message
 *
 * Return value:
 * None.
 */
void
HeliumBootPanic(EFI_STATUS Status, const CHAR16 *fmt, ...)
{
	EFI_EVENT ev;
	EFI_INPUT_KEY Key;
	UINTN index;
	va_list va;

	va_start(va, fmt);

	if (VideoInitFlag) {
		CHAR16 Buffer[1024];

		PrintToScreen(L"HeliumBoot panic: ");
		UnicodeVSPrint(Buffer, sizeof(Buffer), fmt, va);
		PrintToScreen(Buffer);
		PrintToScreen(L"\nEFI status: 0x%X (%r)\n", Status, Status);
		PrintToScreen(L"Press any key to restart...\n");
	} else {
		Print(L"HeliumBoot panic: ");
		VPrint(fmt, va);
		Print(L"\nEFI status: 0x%X (%r)\n", Status, Status);
		Print(L"Press any key to restart...\n");
	}

	va_end(va);

	ev = ST->ConIn->WaitForKey;
	uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ev, &index);
	uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);

	// Infinite loop in case we fail to restart.
	for (;;)
		;
}

UINT32
SwapBytes32(UINT32 val)
{
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

UINT16
SwapBytes16(UINT16 val)
{
    return (val >> 8) | (val << 8);
}

UINT64
GetTimeSeconds(void)
{
	EFI_STATUS Status;
	EFI_TIME Time;

	Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
	if (EFI_ERROR(Status))
		return 0;

	return (UINT64)Time.Hour * 3600 + (UINT64)Time.Minute * 60 + (UINT64)Time.Second;
}

void *
MemMove(void *dst, const void *src, UINTN len)
{
	UINTN i;
	UINT8 *d = (UINT8 *)dst;
	const UINT8 *s = (const UINT8 *)src;

	if (d == s || len == 0)
		return dst;

	if (d < s) {
		// Forward copy
		for (i = 0; i < len; i++)
			d[i] = s[i];
	} else {
		// Backward copy
		for (i = len; i != 0; i--)
			d[i - 1] = s[i - 1];
	}

	return dst;
}

void *
MemCopy(void *Dest, const void *Src, UINTN Length)
{
	UINT8 *d = (UINT8 *)Dest;
	const UINT8 *s = (const UINT8 *)Src;

	while (Length--)
		*d++ = *s++;

	return Dest;
}

CHAR16 *
GetScreenInfo(void)
{
	UINTN width, height;
	CHAR16 *Buffer;

	Buffer = AllocatePool(128 * sizeof(CHAR16));
	if (!Buffer)
		return NULL;

	GetScreenSize(&width, &height);

	if (FramebufferAllowed) {
		UnicodeSPrint(Buffer, 128 * sizeof(CHAR16), L"Using UEFI Framebuffer, %lux%lu", width, height);
		return Buffer;
	} else {
		UnicodeSPrint(Buffer, 128 * sizeof(CHAR16), L"Using BLT, %lux%lu", width, height);
		return Buffer;
	}
}

#if defined(X86_64_BLD)
void
AsmCpuid(UINT32 Leaf, UINT32 subleaf, UINT32 *Eax, UINT32 *Ebx, UINT32 *Ecx, UINT32 *Edx)
{
	UINT32 a, b, c, d;

	__asm__ volatile (
		"xchg %%ebx, %1\n\t"
		"cpuid\n\t"
		"xchg %%ebx, %1\n\t"
		: "=a"(a), "=&r"(b), "=c"(c), "=d"(d)
		: "a"(Leaf), "c"(subleaf)
		: "cc"
	);

	if (Eax)
		*Eax = a;
	if (Ebx)
		*Ebx = b;
	if (Ecx)
		*Ecx = c;
	if (Edx)
		*Edx = d;
}
#endif

UINT64
GetTotalMemoryBytes(void)
{
	EFI_STATUS Status;
	EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
	EFI_MEMORY_DESCRIPTOR *Desc;
	UINTN MemMapSize = 0;
	UINTN MapKey;
	UINTN DescSize;
	UINT32 DescVersion;
	UINT64 Total = 0;
	UINTN i;

	// Initial probe.
	Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, NULL,
		&MapKey, &DescSize, &DescVersion);
	if (Status != EFI_BUFFER_TOO_SMALL)
		return 0;

	/*
	 * Some computers with older firmware REQUIRE more space.
	 * This code has been tested with an early 2009 A1181 MacBook,
	 * with EFI revision 1.10.
	 */
	MemMapSize += 2 * DescSize;

	for (;;) {
		MemMap = AllocatePool(MemMapSize);
		if (!MemMap)
			return 0;
		Status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap,
			&MapKey, &DescSize, &DescVersion);
		if (Status == EFI_BUFFER_TOO_SMALL) {
			FreePool(MemMap);
			MemMapSize += 2 * DescSize;
			continue;
		}
		if (EFI_ERROR(Status)) {
			FreePool(MemMap);
			return 0;
		}
		break;
	}

	for (i = 0; i < MemMapSize / DescSize; i++) {
		Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemMap + (i * DescSize));
		if (Desc->Type == EfiConventionalMemory)
			Total += Desc->NumberOfPages * EFI_PAGE_SIZE;
	}

	FreePool(MemMap);
	return Total;
}

CHAR16 *
StrStr(const CHAR16 *haystack, const CHAR16 *needle)
{
	if (!haystack || !needle)
		return NULL;

	if (*needle == L'\0')
		return (CHAR16 *)haystack;

	while (*haystack != L'\0') {
		const CHAR16 *h = haystack;
		const CHAR16 *n = needle;

		while (*h != L'\0' && *n != L'\0' && *h == *n) {
			h++;
			n++;
		}

		if (*n == L'\0')
			return (CHAR16 *)haystack;

		haystack++;
	}

	return NULL;
}

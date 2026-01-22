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
 *
 * In addition, some code is derived from GNU-EFI, adapted for this
 * graphics renderer. Specifically, the functions "InputToScreen()" and
 * "InputToScreen_Internal()".
 *
 * Copyright (c) 1998-2000 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 * the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. THE EFI SPECIFICATION AND ALL OTHER INFORMATION
 * ON THIS WEB SITE ARE PROVIDED "AS IS" WITH NO WARRANTIES, AND ARE SUBJECT
 * TO CHANGE WITHOUT NOTICE.
 */

#include <efi.h>
#include <efilib.h>

#include <stdarg.h>

#include "boot.h"
#include "config.h"
#include "font.h"

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
static EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Shadow;

UINT8 *Screen;
static UINTN Screenwidth;
static UINTN Screenheight;
UINTN PixelsPerScanLine;
UINTN PixelSize = 4;
INTN XPos;
INTN YPos;
UINTN Colours;

UINTN Progress[2];
UINTN Pixels[2];
UINTN Limit[2];
UINT32 ProgressTime[2];
UINT32 StartTime;

BOOLEAN FramebufferAllowed = TRUE;
BOOLEAN VideoInitFlag = FALSE;

const UINTN KRgbBlack		= 0x000000;
const UINTN KRgbDarkGray	= 0x555555;
const UINTN KRgbDarkRed		= 0x800000;
const UINTN KRgbDarkGreen	= 0x008000;
const UINTN KRgbDarkYellow	= 0x808000;
const UINTN KRgbDarkBlue	= 0x000080;
const UINTN KRgbDarkMagenta	= 0x800080;
const UINTN KRgbDarkCyan	= 0x008080;
const UINTN KRgbRed			= 0xFF0000;
const UINTN KRgbGreen		= 0x00FF00;
const UINTN KRgbYellow		= 0xFFFF00;
const UINTN KRgbBlue		= 0x0000FF;
const UINTN KRgbMagenta		= 0xFF00FF;
const UINTN KRgbCyan		= 0x00FFFF;
const UINTN KRgbGray		= 0xAAAAAA;
const UINTN KRgbWhite		= 0xFFFFFF;
const UINTN KRgbDimWhite    = 0xCCCCCC;

const UINT32 Palette32[16] = {
    KRgbBlack,       // 0
    KRgbDarkBlue,    // 1
    KRgbDarkGreen,   // 2
    KRgbDarkCyan,    // 3
    KRgbDarkRed,     // 4
    KRgbDarkMagenta, // 5
    KRgbDarkYellow,  // 6
    KRgbDimWhite,    // 7
    KRgbGray,        // 8
    KRgbBlue,        // 9
    KRgbDarkGreen,   // 10
    KRgbCyan,        // 11
    KRgbRed,         // 12
    KRgbMagenta,     // 13
    KRgbYellow,      // 14
    KRgbWhite        // 15
};

const UINT8 KForeground = 15;
const UINT8 KBackground = 9;
const UINTN PColour[2]= { 0xa08, 0xc08 };
const UINTN IPColour[2]= { 0x809, 0x809 };

#define FONT_WIDTH   10
#define FONT_HEIGHT  18
#define FONT_CHAR_WIDTH	10
#define FONT_CHAR_HEIGHT	18
#define NUM_FONTS (FONT_ATLAS_WIDTH / FONT_CHAR_WIDTH)
#define FONT_ATLAS_WIDTH   960
#define ROW_BYTES          ((FONT_ATLAS_WIDTH + 7) / 8)		// unused for now

#define SCREEN_SIZE		(Screenheight * Screenwidth * PixelSize)	// number of pixels (size in bytes)
#define MAX_COLUMN		(Screenwidth/FONT_WIDTH)	// chars per line e.g. 80 or 40
#define MAX_ROW		(Screenheight/FONT_HEIGHT)	// lines per screen e.g. 48 or 24
#define PROGRESSBAR0_ROW (MAX_ROW-1)	// row in which to draw progress bar 0
#define PROGRESSBAR1_ROW (MAX_ROW-2)	// row in which to draw progress bar 1
#define STATS_ROW (MAX_ROW-3)	// DEBUGGING row in which to write eta/bps

static UINT8 FontBitmap[FONT_HEIGHT][FONT_ATLAS_WIDTH];

static void InputToScreen_Internal(SIMPLE_INPUT_INTERFACE *ConIn,
	CHAR16 *Prompt, CHAR16 *InStr, UINTN StrLen);

static void
DecodeFontRLE(UINT8 *dst, size_t dst_len)
{
    const UINT8 *in = font.rundata;
    UINT8 data;
    size_t written = 0;

    while ((data = *in++) && written < dst_len) {
        UINT8 value = (data & 0x80) ? 255 : 0;
        UINT8 count = data & 0x7F;
        if (written + count > dst_len)
            count = dst_len - written;

        MemSet(dst + written, value, count);
        written += count;
    }
}

void
FlushScreen(void)
{
	EFI_STATUS Status;

	if (!Shadow || !gop || FramebufferAllowed)
		return;

	Status = uefi_call_wrapper(gop->Blt, 10, gop, Shadow, EfiBltBufferToVideo,
		0, 0, 0, 0, Screenwidth, Screenheight, Screenwidth * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
	if (EFI_ERROR(Status))
		PrintToScreen(L"FlushScreen() failed: %r\n", Status);
}

static void
ColourPixel(INTN x, INTN y, UINTN Colour)
{
	UINT32 Pixel;
	UINT8 Red, Green, Blue;

	Pixel = Palette32[Colour];
	Red = (Pixel >> 16) & 0xFF;
	Green = (Pixel >> 8) & 0xFF;
	Blue = Pixel & 0xFF;

	if (!FramebufferAllowed) {
		// Write to shadow buffer.
		Shadow[y * Screenwidth + x].Red = Red;
		Shadow[y * Screenwidth + x].Green = Green;
		Shadow[y * Screenwidth + x].Blue = Blue;
		Shadow[y * Screenwidth + x].Reserved = 0;
	} else {
		UINT8 *Fb = Screen;
		Fb = Screen + (y * PixelsPerScanLine + x) * PixelSize;

		switch (gop->Mode->Info->PixelFormat) {
			case PixelBlueGreenRedReserved8BitPerColor:
				Fb[0] = Blue;
				Fb[1] = Green;
				Fb[2] = Red;
				break;
			case PixelRedGreenBlueReserved8BitPerColor:
				Fb[0] = Red;
				Fb[1] = Green;
				Fb[2] = Blue;
				break;
			case PixelBitMask:
				Fb[0] = (Pixel & gop->Mode->Info->PixelInformation.RedMask) >> 16;
				Fb[1] = (Pixel & gop->Mode->Info->PixelInformation.GreenMask) >> 8;
				Fb[2] = Pixel & gop->Mode->Info->PixelInformation.BlueMask;
				break;
			case PixelBltOnly:
				// We have no direct framebuffer access.
				break;
			default:
				// Unsupported pixel format.
				break;
		}

		if (PixelSize == 4)
			Fb[3] = 0;	// reserved
	}
}

void
ClearScreen(void)
{
	INTN x, y;

	for (y = 0; y < Screenheight; y++) {
		for (x = 0; x < Screenwidth; x++)
			ColourPixel(x, y, KBackground);
	}

	XPos = 0;
	YPos = 0;
	Colours = (KForeground << 8) | KBackground;

	if (!FramebufferAllowed)
		FlushScreen();
}

void
DisplayChar(UINT8 Char, UINTN x, UINTN y, UINTN Colour)
{
    INTN glyph = Char - 32;
    UINTN row, col;

    if (glyph < 0 || glyph >= NUM_FONTS)
        return;

    UINTN base_x = glyph * font.cwidth;
    UINTN screen_x = x * FONT_WIDTH;
    UINTN screen_y = y * FONT_HEIGHT;

    for (row = 0; row < font.cheight; row++) {
        for (col = 0; col < font.cwidth; col++) {
            UINT8 pixel = FontBitmap[row][base_x + col];
            ColourPixel(
                screen_x + col,
                screen_y + row,
                pixel ? (Colour >> 8) : (Colour & 0xFF)
            );
        }
    }
}

void
ScrollScreen(void)
{
	UINT8 *Fb = Screen;
	INTN x, y;

	if (!FramebufferAllowed) {
		MemMove(Shadow, Shadow + Screenwidth * FONT_HEIGHT, (Screenheight - FONT_HEIGHT) * Screenwidth * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
		EFI_GRAPHICS_OUTPUT_BLT_PIXEL ClearPixel = { .Red = 0, .Green = 0, .Blue = 0, .Reserved = 0 };
		uefi_call_wrapper(gop->Blt, 10, gop, &ClearPixel, EfiBltVideoFill,
			0, 0, 0, Screenheight - FONT_HEIGHT, Screenwidth, FONT_HEIGHT, 0);
		FlushScreen();
	} else {
		UINTN PixelStride = gop->Mode->Info->PixelsPerScanLine * PixelSize;
		UINTN LineBytes = PixelStride;
		UINTN MoveLines = Screenheight - FONT_HEIGHT;
		MemMove(Fb, Fb + FONT_HEIGHT * LineBytes, MoveLines * LineBytes);
		for (y = 0; y < FONT_HEIGHT; y++) {
			for (x = 0; x < Screenwidth; x++)
				ColourPixel(x, Screenheight - FONT_HEIGHT + y, KBackground);
		}
	}
}

void
ScrollScreenDown(void)
{
	INTN x, y;
	UINT8 *Fb = Screen;

	if (!FramebufferAllowed) {
		MemMove(Shadow + Screenwidth, Shadow, (Screenheight - 1) * Screenwidth * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
		EFI_GRAPHICS_OUTPUT_BLT_PIXEL ClearPixel = {0, 0, 0, 0};
		uefi_call_wrapper(gop->Blt, 10, gop, &ClearPixel, EfiBltVideoFill,
			0, 0, 0, 0, Screenwidth, FONT_HEIGHT, 0);
		FlushScreen();
	} else {
		UINTN PixelStride = gop->Mode->Info->PixelsPerScanLine * PixelSize;
		UINTN LineBytes = PixelStride;
		UINTN MoveLines = Screenheight - FONT_HEIGHT;
		MemMove(Fb + FONT_HEIGHT * LineBytes, Fb, MoveLines * LineBytes);
		for (y = 0; y < FONT_HEIGHT; y++) {
			for (x = 0; x < Screenwidth; x++)
				ColourPixel(x, y, KBackground);
		}
	}
}

static void
CurDown(void)
{
	if (++YPos == (MAX_ROW - 3)) {
		YPos = (MAX_ROW - 4);
		ScrollScreen();
	}
}

static void
CurUp(void)
{
	if (--YPos < 0) {
		YPos = 0;
		ScrollScreenDown();
	}
}

static void
CurRight(void)
{
	if (++XPos == MAX_COLUMN) {
		XPos = 0;
		CurDown();
	}
}

static void
CurLeft(void)
{
	if (--XPos < 0) {
		XPos = MAX_COLUMN - 1;
		CurUp();
	}
}

void
SetPos(INTN X, INTN Y)
{
	XPos = X;
	YPos = Y;
}

void
PutChar(UINT8 Char)
{
	switch (Char) {
		case 8:
			CurLeft();
			return;
		case 9:
			CurRight();
			return;
		case 10:
			CurDown();
			return;
		case 11:
			CurUp();
			return;
		case 12:
			ClearScreen();
			return;
		case 13:
			XPos = 0;
			return;
		default:
			break;
	}

	DisplayChar(Char, XPos, YPos, Colours);
	if (++XPos == MAX_COLUMN) {
		XPos = 0;
		if (++YPos == (MAX_ROW - 3)) {
			YPos = (MAX_ROW - 4);
			ScrollScreen();
		}
	}
}

void
PutString(const CHAR8 *s)
{
	while (*s)
		PutChar(*s++);

	// Flush only once per string.
	if (!FramebufferAllowed)
		FlushScreen();
}

void
PrintToScreen(const CHAR16 *Fmt, ...)
{
	INTN i;
	CHAR16 Buffer[1024];
	CHAR8 AsciiBuffer[1024];
	va_list va;

	va_start(va, Fmt);

	if (VideoInitFlag && !UseUefiConsole) {
		UnicodeVSPrint(Buffer, sizeof(Buffer), Fmt, va);
		va_end(va);

		// Convert the string to ASCII for PutString().
		for (i = 0; Buffer[i] != L'\0' && i < sizeof(AsciiBuffer) - 1; i++)
			AsciiBuffer[i] = (Buffer[i] < 0x80) ? (CHAR8)Buffer[i] : '?';

		AsciiBuffer[i] = '\0';
		PutString(AsciiBuffer);
	} else {
		VPrint(Fmt, va);
		va_end(va);
	}
}

void
InputToScreen(CHAR16 *Prompt, CHAR16 *InStr, UINTN StrLen)
{
	if (!UseUefiConsole)
		InputToScreen_Internal(ST->ConIn, Prompt, InStr, StrLen);
	else
		Input(Prompt, InStr, StrLen);
}

static void
InputToScreen_Internal(SIMPLE_INPUT_INTERFACE *ConIn, CHAR16 *Prompt,
	CHAR16 *InStr, UINTN StrLen)
{
	EFI_STATUS Status;
	EFI_INPUT_KEY Key;
	UINTN Len;

	if (Prompt)
		PrintToScreen(L"%s", Prompt);

	Len = 0;

	for (;;) {
		WaitForSingleEvent(ConIn->WaitForKey, 0);

		Status = uefi_call_wrapper(ConIn->ReadKeyStroke, 2, ConIn, &Key);
		if (EFI_ERROR(Status))
			break;

		// Handle Enter keypress.
		if (Key.UnicodeChar == L'\r' || Key.UnicodeChar == L'\n')
			break;

		// Handle Backspace keypress.
		if (Key.UnicodeChar == L'\b') {
			if (Len) {
				Len--;
				InStr[Len] = L'\0';
				PutChar('\b');
				PutChar(' ');
				PutChar('\b');
			}
			continue;
		}

		if (Key.UnicodeChar >= L' ') {
			if (Len < StrLen - 1) {
				InStr[Len++] = Key.UnicodeChar;
				InStr[Len] = L'\0';

				if (Key.UnicodeChar < 0x80)
					PutChar((CHAR8)Key.UnicodeChar);
				else
					PutChar('?');
			}
		}
	}

	InStr[Len] = L'\0';
}

void
InitProgressBar(INTN Id, UINTN BarLimit, CONST CHAR8 *Buffer)
{
	CHAR8 c;
	INTN x = 0;
	INTN line = Id ? PROGRESSBAR0_ROW : PROGRESSBAR1_ROW;

	Progress[Id] = 0;
	if (Id == 0)
		StartTime = 0;

	Pixels[Id] = 0;
	Limit[Id] = BarLimit;
	while ((c = *Buffer++) != 0 && x < 7) {
		DisplayChar(c, x, line, Colours);
		++x;
	}

	if (x < 7)
		x = 7;

	for (; x < (MAX_COLUMN - 1); ++x)
		DisplayChar(0x7f, x, line, IPColour[Id]);
}

void
UpdateProgressBar(INTN Id, UINTN NewProgress)
{
	INTN i;
	INTN line = Id ? PROGRESSBAR0_ROW : PROGRESSBAR1_ROW;
	UINTN old_pixels = Pixels[Id];

#if defined(DEBUG_BLD)
	if (Id == 0) {
		UINT64 timenow;

		 if (StartTime == 0) {
			 StartTime = GetTimeSeconds();
			 ProgressTime[Id] = StartTime;
		 }

		UINT32 avg_bps = 0;
		UINT32 bps = 0;
		UINT32 eta = 0;

		timenow = GetTimeSeconds();

		UINT64 delta_time = timenow - ProgressTime[Id];
		ProgressTime[Id] = timenow;

		if (delta_time) {
			bps = ((NewProgress - Progress[Id]) * 1000) / delta_time;
			delta_time = timenow - StartTime;
			if (delta_time) {
				avg_bps = ((UINT64)NewProgress * 1000) / delta_time;
				if (avg_bps)
					eta = (Limit[Id] - NewProgress) / avg_bps;
			}
		}

		INTN savedXPos = XPos;
		INTN savedYPos = YPos;

		XPos = 0;
		YPos = STATS_ROW;

		PrintToScreen(L"point: %7u ETA: %u    ", bps, eta);

		XPos = savedXPos;
		YPos = savedYPos;
	}
#endif

	Progress[Id] = NewProgress;
	INT64 prog64 = NewProgress;
	prog64 *= (Screenwidth - (8 * FONT_WIDTH));
	prog64 /= Limit[Id];
	UINTN pixels = (UINTN)prog64;
	if (pixels > old_pixels) {
		Pixels[Id] = pixels;
		while (old_pixels < pixels) {
			for (i = 0; i < 6; ++i)
				ColourPixel(old_pixels + (7*FONT_WIDTH), (line * FONT_HEIGHT) + 2 + i, (PColour[Id] >> 8));
			++old_pixels;
		}
	}
}

void
GetScreenSize(UINTN *ScreenWidth, UINTN *ScreenHeight)
{
	*ScreenWidth = Screenwidth;
	*ScreenHeight = Screenheight;
}

EFI_STATUS
InitVideo(void)
{
	EFI_STATUS Status;
	EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
	UINT32 ModeNumber, Res;
	UINTN InfoSize;
	UINT32 MaxRes = 0;
	UINT32 BestMode = 0;

	// Locate the GOP.
	Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GraphicsOutputProtocolGuid, NULL, (void **)&gop);
	if (EFI_ERROR(Status)) {
		Print(L"InitVideo: Failed to locate GOP: %r\n", Status);
		gop = NULL;
		return Status;
	}

	// Find the best video mode.
	for (ModeNumber = 0; ModeNumber < gop->Mode->MaxMode; ModeNumber++) {
		Status = uefi_call_wrapper(gop->QueryMode, 4, gop, ModeNumber, &InfoSize, &Info);
		if (!EFI_ERROR(Status)) {
			Res = Info->HorizontalResolution * Info->VerticalResolution;
			if (Res > MaxRes) {
				MaxRes = Res;
				BestMode = ModeNumber;
			}
		}
	}

	// Reset console output.
	Status = uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to reset console: %r\n", Status);
		return Status;
	}

	// Set the video mode.
	Status = uefi_call_wrapper(gop->SetMode, 2, gop, BestMode);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return Status;
	}

	// Enable the UEFI blinking text cursor.
	Status = uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to enable the cursor: %r\n", Status);
		return Status;
	}

	// Set text color attributes.
	Status = uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set text color attributes: %r\n", Status);
		return Status;
	}

	// Clear the screen.
	Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to clear screen: %r\n", Status);
		return Status;
	}

	Screenwidth = gop->Mode->Info->HorizontalResolution;
	Screenheight = gop->Mode->Info->VerticalResolution;
	PixelsPerScanLine = gop->Mode->Info->PixelsPerScanLine;

	switch (gop->Mode->Info->PixelFormat) {
		case PixelBlueGreenRedReserved8BitPerColor:
			PixelSize = 4;
			break;
		case PixelRedGreenBlueReserved8BitPerColor:
			PixelSize = 4;
			break;
		case PixelBitMask:
			PixelSize = 4;		// This is usually the case.
			break;
		case PixelBltOnly:		// We cannot access the framebuffer directly.
#if defined(DEBUG_BLD)
			Print(L"GOP is BLT-only - direct framebuffer access disabled!\n");
#endif
			FramebufferAllowed = FALSE;

			// Allocate shadow buffer.
			Shadow = AllocateZeroPool(Screenwidth * Screenheight * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
			if (!Shadow)
				return EFI_OUT_OF_RESOURCES;
			break;
		default:
			Print(L"Unsupported pixel format %lu!\n", gop->Mode->Info->PixelFormat);
			return EFI_UNSUPPORTED;
	}

	if (FramebufferAllowed)
		Screen = (UINT8 *)gop->Mode->FrameBufferBase;

	DecodeFontRLE((UINT8 *)FontBitmap, sizeof(FontBitmap));
	ClearScreen();

	VideoInitFlag = TRUE;

#if defined(DEBUG_BLD)
	UINTN cols = Info->HorizontalResolution / 8;
	UINTN rows = Info->VerticalResolution / 16;

	PrintToScreen(L"GOP initialized: %ux%u, %u pixels/scanline\n", gop->Mode->Info->HorizontalResolution,
		gop->Mode->Info->VerticalResolution, gop->Mode->Info->PixelsPerScanLine);
	PrintToScreen(L"Framebuffer base: 0x%lx, size: %lu bytes\n", gop->Mode->FrameBufferBase,
		gop->Mode->FrameBufferSize);
	PrintToScreen(L"Text mode: %ux%u\n", cols, rows);
#endif

	return EFI_SUCCESS;
}

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
#include "cmd.h"
#include "menu.h"

INTN mainmenu_items = 0;
INTN mainmenu_idx = 0;

BOOLEAN gMenuExit = FALSE;
BOOLEAN gScreenUpdate;

static void MenuDisplayFn(void);
static void MenuProcessKey(EFI_INPUT_KEY *Key);

struct DisplayProcess Screens[] = {
	{MenuDisplayFn, MenuProcessKey},
	{NULL, NULL}
};

#define SCREEN_MENU	0

struct DisplayProcess *gCurrentScreen = &Screens[SCREEN_MENU];

static void
ExitToCommandMonitor(UINT32 Dummy1, UINT32 Dummy2)
{
	gMenuExit = TRUE;
}

static void
MenuRebootSystem(UINT32 Dummy1, UINT32 Dummy2)
{
	EFI_STATUS Status;

	Status = uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
	if (EFI_ERROR(Status))
		PrintToScreen(L"Reset failed: %r\n", Status);
}

struct MenuItem MainMenu[] = {
	{ L"Enter Command Monitor", ExitToCommandMonitor, 0, 0},
	{ L"Reboot system", MenuRebootSystem, 0, 0},
	{ L"The End", NULL, 0, 0 }
};

static void
MenuDisplayFn(void)
{
	CHAR16 Buf[8];
	INTN i;

	ClearScreen();

#if defined(DEV_BLD) || defined(DEBUG_BLD)
	PrintToScreen(L"Project Kyasarin main menu - Revision %s, %s\n", getrevision(), getrevdate());
#else
	PrintToScreen(L"HeliumBoot main menu - Version %s, %s\n", getvernum(), getdate());
#endif

	mainmenu_items = 0;
	for (i = 0; MainMenu[i].MenuFunc != NULL; i++) {
		SetPos(0, i + 1);
		if (mainmenu_idx == i)
			PrintToScreen(L"==> ");
		else
			PrintToScreen(L"    ");

		UnicodeSPrint(Buf, sizeof(Buf), L"%d", i);
		PrintToScreen(Buf);
		PrintToScreen(L".");
		PrintToScreen(MainMenu[i].Name);
		mainmenu_items++;
	}

	SetPos(0, 14);
	PrintToScreen(L"============================");
}

static void
MenuProcessKey(EFI_INPUT_KEY *Key)
{
	if (Key->UnicodeChar == CHAR_CARRIAGE_RETURN) {
		MainMenu[mainmenu_idx].MenuFunc(MainMenu[mainmenu_idx].Param1,
			MainMenu[mainmenu_idx].Param2);
		return;
	}

	if (Key->ScanCode == SCAN_DOWN) {
		if (++mainmenu_idx == mainmenu_items)
			mainmenu_idx = 0;
		gScreenUpdate = TRUE;
		return;
	}

	if (Key->ScanCode == SCAN_UP) {
		if (--mainmenu_idx < 0)
			mainmenu_idx = mainmenu_items - 1;
		gScreenUpdate = TRUE;
		return;
	}

	if (Key->UnicodeChar >= L'0' && Key->UnicodeChar <= L'9') {
		UINTN num = Key->UnicodeChar - L'0';
		if (num < (UINTN)mainmenu_items)
			MainMenu[num].MenuFunc(MainMenu[num].Param1, MainMenu[num].Param2);
	}
}

void
StartMenu(void)
{
	EFI_STATUS Status;
	EFI_INPUT_KEY Key;
	EFI_EVENT Events[1];
	UINTN Index;

	// Make sure the cursor is disabled on the UEFI side, even though
	// our graphics renderer doesn't have a visible cursor.
	uefi_call_wrapper(gST->ConOut->EnableCursor, 2, gST->ConOut, FALSE);

	gCurrentScreen->DisplayFn();
	Events[0] = gST->ConIn->WaitForKey;

	while (!gMenuExit) {
		uefi_call_wrapper(gBS->WaitForEvent, 3, 1, Events, &Index);
		Status = uefi_call_wrapper(gST->ConIn->ReadKeyStroke, 2, gST->ConIn, &Key);
		if (EFI_ERROR(Status))
			continue;
		gScreenUpdate = FALSE;
		gCurrentScreen->ProcessKey(&Key);

		if (gMenuExit) {
			gMenuExit = FALSE;
			CommandMonitor();
			gScreenUpdate = TRUE;
		}

		if (gScreenUpdate)
			gCurrentScreen->DisplayFn();
	}
}

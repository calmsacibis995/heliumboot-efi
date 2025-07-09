#include <efi.h>
#include <efilib.h>

#include "boot.h"

void
echo(CHAR16 *args)
{
	Print(L"%s\n", args);
}

void
ls(CHAR16 *args)
{
	Print(L"Not implemented yet!\n");
}

void
print_version(CHAR16 *args)
{
	Print(L"HeliumBoot %s\n", version);
}

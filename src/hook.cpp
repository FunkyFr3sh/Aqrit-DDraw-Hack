#include <windows.h>
#include <stdio.h>
#include "hook.h"

void Hook_PatchIAT(HMODULE hMod, char *moduleName, char *functionName, int ordinal, PROC newFunction)
{
	if (!hMod || hMod == INVALID_HANDLE_VALUE || !newFunction)
		return;

	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hMod;
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	PIMAGE_NT_HEADERS pNTHeaders = (PIMAGE_NT_HEADERS)((DWORD)pDosHeader + (DWORD)pDosHeader->e_lfanew);
	if (pNTHeaders->Signature != IMAGE_NT_SIGNATURE)
		return;

	PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor =
		(PIMAGE_IMPORT_DESCRIPTOR)((DWORD)pDosHeader +
		(DWORD)(pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

	if (pImportDescriptor == (PIMAGE_IMPORT_DESCRIPTOR)pNTHeaders)
		return;

	while (pImportDescriptor->FirstThunk)
	{
		char *impModuleName = (char *)((DWORD)pDosHeader + (DWORD)(pImportDescriptor->Name));

		if (_stricmp(impModuleName, moduleName) == 0)
		{
			PIMAGE_THUNK_DATA pFirstThunk =
				(PIMAGE_THUNK_DATA)((DWORD)pDosHeader + (DWORD)pImportDescriptor->FirstThunk);

			PIMAGE_THUNK_DATA pOrigFirstThunk =
				(PIMAGE_THUNK_DATA)((DWORD)pDosHeader + (DWORD)pImportDescriptor->OriginalFirstThunk);

			while (pFirstThunk->u1.Function && pOrigFirstThunk->u1.AddressOfData)
			{
				PIMAGE_IMPORT_BY_NAME pImport =
					(PIMAGE_IMPORT_BY_NAME)((DWORD)pDosHeader + pOrigFirstThunk->u1.AddressOfData);

				BOOL isOrdinal = (pOrigFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0;

				if ((!isOrdinal && _stricmp((const char *)pImport->Name, functionName) == 0) ||
					(isOrdinal && ordinal && IMAGE_ORDINAL(pOrigFirstThunk->u1.Ordinal) == ordinal))
				{
					DWORD oldProtect;

					if (VirtualProtect(&pFirstThunk->u1.Function, sizeof(DWORD), PAGE_READWRITE, &oldProtect))
					{
						pFirstThunk->u1.Function = (DWORD)newFunction;
						VirtualProtect(&pFirstThunk->u1.Function, sizeof(DWORD), oldProtect, &oldProtect);
					}

					break;
				}

				pFirstThunk++;
				pOrigFirstThunk++;
			}
		}

		pImportDescriptor++;
	}
}

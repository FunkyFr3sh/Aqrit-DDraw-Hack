#include <windows.h>
#include <stdio.h>
#include "detours.h"
#include "hook.h"

void Hook_Create(PROC newFunction, PROC *function)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach((PVOID *)function, (PVOID)newFunction);
	DetourTransactionCommit();
}

void Hook_Revert(PROC newFunction, PROC *function)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach((PVOID *)function, (PVOID)newFunction);
	DetourTransactionCommit();
}

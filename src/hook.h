#ifndef HOOK_H
#define HOOK_H

void Hook_Create(PROC newFunction, PROC *function);
void Hook_Revert(PROC newFunction, PROC *function);
void Hook_PatchIAT(HMODULE hMod, char *moduleName, char *functionName, int ordinal, PROC newFunction);

#endif

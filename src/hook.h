#ifndef HOOK_H
#define HOOK_H

void Hook_PatchIAT(HMODULE hMod, char *moduleName, char *functionName, int ordinal, PROC newFunction);

#endif

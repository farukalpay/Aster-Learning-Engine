#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_WIN32_MODULE)

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void* _desktop_windowPlatformLoadModule(const char* path)
{
    return LoadLibraryA(path);
}

void _desktop_windowPlatformFreeModule(void* module)
{
    FreeLibrary((HMODULE) module);
}

DESKTOP_WINDOWproc _desktop_windowPlatformGetModuleSymbol(void* module, const char* name)
{
    return (DESKTOP_WINDOWproc) GetProcAddress((HMODULE) module, name);
}

#endif // DESKTOP_WINDOW_BUILD_WIN32_MODULE


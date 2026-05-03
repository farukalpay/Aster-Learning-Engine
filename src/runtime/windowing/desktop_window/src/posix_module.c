#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_POSIX_MODULE)

#include <dlfcn.h>

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void* _desktop_windowPlatformLoadModule(const char* path)
{
    return dlopen(path, RTLD_LAZY | RTLD_LOCAL);
}

void _desktop_windowPlatformFreeModule(void* module)
{
    dlclose(module);
}

DESKTOP_WINDOWproc _desktop_windowPlatformGetModuleSymbol(void* module, const char* name)
{
    return dlsym(module, name);
}

#endif // DESKTOP_WINDOW_BUILD_POSIX_MODULE


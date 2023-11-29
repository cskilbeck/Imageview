//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "WinHTTP.lib")
#pragma comment(lib, "Comctl32.lib")

#pragma comment(linker,                                                                                         \
                "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0'" \
                " processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

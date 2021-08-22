#pragma once

// ImageView is tiny so stuffing all the headers into a single precompiled header makes sense

#include <winsdkver.h>

// Support Windows 7

// To use Windows 8/10 features, well...
// One option would be to constrain all uses of those functions to a single source file,
// and in there, define _WIN32_WINNT etc and switch off precompiled headers for that file
// Then very carefully use those features after checking the operating system version

// Big hassle but if there's a compelling feature then it might be worth it I guess?

// _WIN32_WINNT/NTDDI_VERSION are defined at project scope because some modules don't use PCH

//#define _WIN32_WINNT _WIN32_WINNT_WIN7
//#define NTDDI_VERSION NTDDI_WIN7

// MediaFoundation guids are required for checking HEIF support

#define MF_INIT_GUIDS

// All the Windows

#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <hidusage.h>
#include <wrl/client.h>
#include <comdef.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <wincodec.h>
#include <Commctrl.h>
#include <Shlobj.h>
#include <mfapi.h>
#include <d2d1.h>
#include <dwrite_1.h>
#include <strsafe.h>
#include <shobjidl_core.h>
#include <winhttp.h>
#include <wincodec.h>

// Old std lib

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <tchar.h>

// New std lib

#include <string>
#include <vector>
#include <mutex>
#include <locale>
#include <codecvt>
#include <functional>
#include <thread>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <array>

// Resource IDs

#include "../resources/resource.h"

// Local

#include "util.h"
#include "log.h"
#include "rect.h"
#include "WICTextureLoader11.h"
#include "DragDropHelpers.h"
#include "font_loader.h"
#include "new_windows.h"
#include "timer.h"

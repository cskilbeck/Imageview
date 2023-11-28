//////////////////////////////////////////////////////////////////////
// ImageView is tiny so stuffing all the headers into a single precompiled header makes sense

#pragma once

//////////////////////////////////////////////////////////////////////
// TODO(chs): replace codecvt stuff with MultiByteToWideChar and WideCharToMultiByte

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

//////////////////////////////////////////////////////////////////////
// Logging

#if defined(_DEBUG)
#define LOG_ENABLED 1
#else
#define LOG_ENABLED 0
#endif

//////////////////////////////////////////////////////////////////////
// ANSI not supported

#if !defined(UNICODE)
#error Unicode only
#endif

//////////////////////////////////////////////////////////////////////
// define SLOW_THINGS_DOWN to add some pauses to load_file and scan_folder for testing thread stuff

// #define SLOW_THINGS_DOWN

//////////////////////////////////////////////////////////////////////
// Windows 7 or Windows 8 if DirectComposition is used

#define USE_DIRECTOMPOSITION 0

#if USE_DIRECTCOMPOSITION
#define _WIN32_WINNT _WIN32_WINNT_WIN8
#define NTDDI_VERSION NTDDI_WIN8
#else
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define NTDDI_VERSION NTDDI_WIN7
#endif

#include <winsdkver.h>

//////////////////////////////////////////////////////////////////////
// MediaFoundation guids are required for checking HEIF support

#define MF_INIT_GUIDS

//////////////////////////////////////////////////////////////////////
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
#include <dcomp.h>
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
#include <DirectXMath.h>
#include <propkey.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//////////////////////////////////////////////////////////////////////
// Old std lib

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <tchar.h>

//////////////////////////////////////////////////////////////////////
// New std lib

#include <string>
#include <vector>
#include <mutex>
#include <locale>
#include <codecvt>
#include <functional>
#include <memory>
#include <thread>
#include <set>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <utility>
#include <array>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////
// Resource IDs

#include "../resources/resource.h"

//////////////////////////////////////////////////////////////////////
// Local

#include "types.h"
#include "util.h"
#include "log.h"
#include "defer.h"
#include "rect.h"
#include "files.h"
#include "dialogs.h"
#include "drag_drop.h"
#include "font_loader.h"
#include "new_windows.h"
#include "timer.h"
#include "thread_pool.h"
#include "image_decoder.h"
#include "image.h"
#include "settings_dialog.h"
#include "app.h"
//////////////////////////////////////////////////////////////////////
// ImageView is tiny so stuffing all the headers into a single precompiled header makes sense

#pragma once

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
#error UNICODE only
#endif

//////////////////////////////////////////////////////////////////////
// Set SLOW_THINGS_DOWN to 1 to add some pauses to load_file and scan_folder for testing thread stuff

#define SLOW_THINGS_DOWN 0

//////////////////////////////////////////////////////////////////////
// Set USE_DIRECTOMPOSITION to 1 to use DirectComposition
// This makes Windows 8 the required OS version
//
// We _could_ check OS version and load the DirectComposition stuff
// dynamically but nah

#define USE_DIRECTOMPOSITION 0

//////////////////////////////////////////////////////////////////////

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
#include <windowsx.h>
#include <Uxtheme.h>
#include <CommCtrl.h>
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
#include <directxpackedvector.h>
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
#include <pathcch.h>

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

#include <format>
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
#include <stack>
#include <utility>
#include <array>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////
// Resource IDs

#include "../resources/resource.h"

//////////////////////////////////////////////////////////////////////
// Local

#pragma warning(disable : 4100)

#include "types.h"
#include "util.h"
#include "ansi.h"
#include "log.h"
#include "defer.h"
#include "rect.h"
#include "file.h"
#include "dialogs.h"
#include "drag_drop.h"
#include "font_loader.h"
#include "timer.h"
#include "thread_pool.h"
#include "image.h"
#include "settings.h"
#include "hotkeys.h"
#include "scrollbar.h"

#include "app.h"

#include "setting_controller.h"

#include "tab_page.h"
#include "tab_explorer.h"
#include "tab_hotkeys.h"
#include "tab_about.h"
#include "tab_relaunch.h"
#include "tab_settings.h"

#include "settings_dialog.h"

#include "setting_section.h"
#include "setting_bool.h"
#include "setting_enum.h"
#include "setting_color.h"
#include "setting_ranged.h"

#include "d3d.h"
#include "recent_files.h"

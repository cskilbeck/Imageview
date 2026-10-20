cmake_minimum_required(VERSION 3.21)

project(imageview LANGUAGES CXX)

# Locate fxc.exe from the Windows SDK for HLSL shader compilation
file(GLOB WINSDK_BIN_DIRS "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64")
list(REVERSE WINSDK_BIN_DIRS)
find_program(FXC_EXECUTABLE fxc HINTS ${WINSDK_BIN_DIRS} REQUIRED)

set(SHADER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader_inc")
file(MAKE_DIRECTORY "${SHADER_GENERATED_DIR}")

set(SHADER_COMMON_DEPS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shader_constants.h"
)

function(compile_hlsl_shader SHADER_NAME SHADER_TYPE)
    set(HLSL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_NAME}.hlsl")
    set(HEADER_FILE "${SHADER_GENERATED_DIR}/${SHADER_NAME}.h")
    set(VAR_NAME "${SHADER_NAME}_bin")

    add_custom_command(
        OUTPUT "${HEADER_FILE}"
        COMMAND "${FXC_EXECUTABLE}"
            /nologo
            /E main
            /T ${SHADER_TYPE}
            /Fh "${HEADER_FILE}"
            /Vn "${VAR_NAME}"
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Debug>:/Zi>
            "${HLSL_FILE}"
        DEPENDS "${HLSL_FILE}" ${SHADER_COMMON_DEPS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Compiling HLSL shader ${SHADER_NAME}.hlsl (${SHADER_TYPE})"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    list(APPEND GENERATED_SHADER_HEADERS "${HEADER_FILE}")
    set(GENERATED_SHADER_HEADERS "${GENERATED_SHADER_HEADERS}" PARENT_SCOPE)
endfunction()

compile_hlsl_shader(vs_rectangle vs_5_0)
compile_hlsl_shader(ps_copy_texture ps_5_0)
compile_hlsl_shader(ps_solid ps_5_0)
compile_hlsl_shader(ps_stripe ps_5_0)
compile_hlsl_shader(ps_texture ps_5_0)

set(IMAGEVIEW_SOURCES
    src/app.cpp
    src/d3d.cpp
    src/dialogs.cpp
    src/drag_drop.cpp
    src/file.cpp
    src/file_types_handler.cpp
    src/font_loader.cpp
    src/hotkeys.cpp
    src/image.cpp
    src/log.cpp
    src/pch.cpp
    src/recent_files.cpp
    src/rect.cpp
    src/scrollbar.cpp
    src/settings.cpp
    src/settings_dialog.cpp
    src/setting_bool.cpp
    src/setting_color.cpp
    src/setting_controller.cpp
    src/setting_enum.cpp
    src/setting_ranged.cpp
    src/setting_section.cpp
    src/tab_about.cpp
    src/tab_explorer.cpp
    src/tab_hotkeys.cpp
    src/tab_settings.cpp
    src/thread_pool.cpp
    src/util.cpp
    src/wm_names.cpp
)

set(IMAGEVIEW_HEADERS
    resources/resource.h
    shaders/hlsl.h
    src/ansi.h
    src/app.h
    src/d3d.h
    src/defer.h
    src/dialogs.h
    src/drag_drop.h
    src/file.h
    src/file_types_handler.h
    src/font_loader.h
    src/hotkeys.h
    src/image.h
    src/log.h
    src/pch.h
    src/recent_files.h
    src/rect.h
    src/scrollbar.h
    src/settings.h
    src/settings_dialog.h
    src/settings_fields.h
    src/settings_reset_decls.h
    src/setting_bool.h
    src/setting_color.h
    src/setting_controller.h
    src/setting_enum.h
    src/setting_ranged.h
    src/setting_section.h
    src/shader_constants.h
    src/tab_about.h
    src/tab_explorer.h
    src/tab_hotkeys.h
    src/tab_page.h
    src/tab_relaunch.h
    src/tab_settings.h
    src/thread_pool.h
    src/timer.h
    src/types.h
    src/util.h
)

add_executable(imageview WIN32
    ${IMAGEVIEW_SOURCES}
    ${IMAGEVIEW_HEADERS}
    ${GENERATED_SHADER_HEADERS}
    resources/imageview.rc
    settings.manifest
)

set_target_properties(imageview PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_include_directories(imageview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_precompile_headers(imageview PRIVATE src/pch.h)

target_compile_definitions(imageview PRIVATE
    UNICODE
    _UNICODE
    _HAS_EXCEPTIONS=0
    WIN32
    _WINDOWS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

if(MSVC)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/O[12]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REGEX REPLACE "/Ob[0-2]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    target_compile_options(imageview PRIVATE
        /W4
        /WX
        /EHs-c-
        /GS-
        /arch:AVX
        /fp:precise
        /Gy
        /MP
        $<$<CONFIG:Debug>:/Od /RTC1>
        $<$<NOT:$<CONFIG:Debug>>:/O1 /Ob2 /Oi /Os /Oy /GT /GL>
    )

    target_link_options(imageview PRIVATE
        /SUBSYSTEM:WINDOWS
        /DEBUG
        $<$<CONFIG:Debug>:/INCREMENTAL>
        $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF /LTCG>
    )
endif()

target_link_libraries(imageview PRIVATE
    kernel32
    user32
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
)

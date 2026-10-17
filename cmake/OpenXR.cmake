# The experimental VR mode (docs/vr.md): the Khronos OpenXR loader, loaded by the game at run time only when VR is on, and the
# headless test runtime that stands in for a headset in the tests. The loader and the headers come from the OpenXR.Loader
# package of the KhronosGroup/OpenXR-SDK-Source release (headers Apache-2.0 OR MIT, loader Apache-2.0); the package and the
# two license texts are pinned by their SHA-256 like the upscaler SDKs (cmake/Upscalers.cmake, pt_sdk_files).
if(WIN32)
  set(pt_openxr_default ON)
else()
  set(pt_openxr_default OFF)
endif()
option(PT_OPENXR "Download the Khronos OpenXR loader and build the experimental VR mode" ${pt_openxr_default})

set(PT_OPENXR_VERSION "1.1.63")
set(PT_OPENXR_DIR "${PT_UPSCALER_SDK_DIR}/openxr-${PT_OPENXR_VERSION}")
set(PT_OPENXR_RUNTIME_FILES)
set(PT_OPENXR_NOTICES)
if(PT_OPENXR)
  pt_sdk_files(pt_openxr_package_ok "https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/release-${PT_OPENXR_VERSION}"
    "${PT_OPENXR_DIR}" "OpenXR.Loader.${PT_OPENXR_VERSION}.nupkg|4e5a50a8807ef66f25180ff224e7d8150b594aa8ee4b07590f9ade55a8e98703")
  pt_sdk_files(pt_openxr_license_ok "https://raw.githubusercontent.com/KhronosGroup/OpenXR-SDK-Source/release-${PT_OPENXR_VERSION}"
    "${PT_OPENXR_DIR}" "LICENSE|cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30"
    "LICENSES/MIT.txt|8f25018489d6fe0dec34a352314c38dc146247b7de65735790f4398a92afa84b")
  set(PT_OPENXR_PACKAGE "${PT_OPENXR_DIR}/package")
  if(pt_openxr_package_ok AND NOT EXISTS "${PT_OPENXR_PACKAGE}/include/openxr/openxr.h")
    file(ARCHIVE_EXTRACT INPUT "${PT_OPENXR_DIR}/OpenXR.Loader.${PT_OPENXR_VERSION}.nupkg" DESTINATION "${PT_OPENXR_PACKAGE}")
  endif()
  if(pt_openxr_package_ok AND pt_openxr_license_ok AND EXISTS "${PT_OPENXR_PACKAGE}/include/openxr/openxr.h")
    set(PT_OPENXR_INCLUDE "${PT_OPENXR_PACKAGE}/include")
    if(WIN32 AND EXISTS "${PT_OPENXR_PACKAGE}/native/x64/release/bin/openxr_loader.dll")
      list(APPEND PT_OPENXR_RUNTIME_FILES "${PT_OPENXR_PACKAGE}/native/x64/release/bin/openxr_loader.dll")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      # The pinned NuGet package is Windows-only. Build the same Khronos loader release
      # from its source archive for Linux; the game discovers this beside pt via $ORIGIN.
      FetchContent_Declare(pt_openxr_source
        URL "https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-${PT_OPENXR_VERSION}.tar.gz"
        URL_HASH SHA256=a3b97a36f11abe256a7ea1668a0a468aac9b738e94bea6b468f0ae31ad537a46)
      FetchContent_GetProperties(pt_openxr_source)
      if(NOT pt_openxr_source_POPULATED)
        FetchContent_Populate(pt_openxr_source)
      endif()
      set(BUILD_LOADER ON CACHE BOOL "Build the OpenXR loader" FORCE)
      set(DYNAMIC_LOADER ON CACHE BOOL "Build a shared OpenXR loader" FORCE)
      set(BUILD_API_LAYERS OFF CACHE BOOL "Do not build OpenXR API layers" FORCE)
      set(BUILD_TESTS OFF CACHE BOOL "Do not build OpenXR SDK tests" FORCE)
      set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "Do not build OpenXR conformance tests" FORCE)
      set(BUILD_SDK_TESTS OFF CACHE BOOL "Do not build OpenXR SDK samples" FORCE)
      # This game uses Vulkan. The SDK's unrelated OpenGL sample helper pulls in
      # optional X11 mode-switch libraries, so keep that helper out of this build.
      set(CMAKE_DISABLE_FIND_PACKAGE_OpenGL TRUE)
      add_subdirectory("${pt_openxr_source_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/pt_openxr_source-build" EXCLUDE_FROM_ALL)
      unset(CMAKE_DISABLE_FIND_PACKAGE_OpenGL)
      # The engine intentionally requests the ABI soname, not a link-time-only .so symlink.
      set_target_properties(openxr_loader PROPERTIES VERSION "" SOVERSION "1")
      foreach(game_target IN ITEMS pt pt_release)
        add_dependencies(${game_target} openxr_loader)
        add_custom_command(TARGET ${game_target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:openxr_loader>" "$<TARGET_FILE_DIR:${game_target}>/libopenxr_loader.so.1"
          VERBATIM)
        set_target_properties(${game_target} PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
      endforeach()
      file(WRITE "${CMAKE_BINARY_DIR}/openxr.txt" "PT_OPENXR=ON\nloader=libopenxr_loader.so.1\nversion=${PT_OPENXR_VERSION}\n")
    endif()
    target_include_directories(pt_engine PRIVATE "${PT_OPENXR_INCLUDE}")
    target_compile_definitions(pt_engine PRIVATE PT_WITH_OPENXR=1)
    list(APPEND PT_OPENXR_NOTICES "${PT_OPENXR_DIR}/LICENSE|Khronos_OpenXR_SDK_LICENSE.txt" "${PT_OPENXR_DIR}/LICENSES/MIT.txt|Khronos_OpenXR_SDK_MIT.txt")

    # the headless test runtime (tools/xr_test_runtime): its own folder, so tools/package.py (which ships the DLLs next to the
    # executable) never takes it; tests select it with XR_RUNTIME_JSON=<build>/xr_test_runtime/pt_xr_test_runtime.json
    add_library(pt_xr_test_runtime SHARED tools/xr_test_runtime/xr_test_runtime.cpp)
    target_include_directories(pt_xr_test_runtime PRIVATE "${PT_OPENXR_INCLUDE}" "${stb_SOURCE_DIR}")
    target_link_libraries(pt_xr_test_runtime PRIVATE Vulkan::Headers)
    if(WIN32)
      # XR_KHR_D3D12_enable: the swapchain images are D3D12 resources on the application's device
      target_link_libraries(pt_xr_test_runtime PRIVATE d3d12)
    endif()
    set(PT_XR_TEST_RUNTIME_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}pt_xr_test_runtime${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set_target_properties(pt_xr_test_runtime PROPERTIES OUTPUT_NAME "pt_xr_test_runtime"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xr_test_runtime" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xr_test_runtime")
    if(MSVC)
      target_compile_options(pt_xr_test_runtime PRIVATE /W3 /utf-8 /EHsc)
    endif()
    # a library path with a separator is relative to the manifest (a bare name would be looked for on the system's path)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/xr_test_runtime")
    file(WRITE "${CMAKE_BINARY_DIR}/xr_test_runtime/pt_xr_test_runtime.json"
      "{\n  \"file_format_version\": \"1.0.0\",\n  \"runtime\": {\n    \"name\": \"pt-port headless test runtime\",\n    \"library_path\": \"./${PT_XR_TEST_RUNTIME_NAME}\"\n  }\n}\n")
    add_dependencies(pt pt_xr_test_runtime)
  else()
    message(FATAL_ERROR "PT_OPENXR=ON requires the pinned OpenXR headers, loader and notices")
  endif()
endif()

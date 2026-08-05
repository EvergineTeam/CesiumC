# Overlay triplet: wasm32-emscripten, Release-only, and without the shared-memory flags.
#
# Overrides vcpkg's built-in wasm32-emscripten. Two differences from it, and one from
# cesium-native's own wasm32-emscripten-cesium, which this is otherwise modelled on.
#
# Release-only, like the other overlay triplets here: vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
#
# No -pthread and no -fwasm-exceptions. cesium-native's triplet sets both, for a configuration
# tuned for Unity, and both are all-or-nothing across a link: -pthread changes the memory model to
# shared and -fwasm-exceptions picks an exception scheme, so every object in the final binary has
# to agree. .NET's browser-wasm does not use either -- measured, not assumed: ImGui.Net's
# cimgui.a and Evergine's ktx.a, the two natives that link into a .NET wasm app today, contain
# zero references to __cpp_exception, __wasm_lpad_context or _Unwind_CallPersonality. An archive
# that carried them would force every consumer to enable wasm exceptions too.
#
# The dependency chain does not need them either. Run 31004829159 built cesium-native's 22
# libraries and its whole vcpkg set three ways -- with the Unity flags, without threads and
# exceptions, and with no extra flags at all -- and got identical results: same 28 packages, same
# 258 targets, same 22 archives.
#
# The wasm feature flags below are kept because they are not all-or-nothing: they widen the
# instruction set the compiler may emit and cost nothing to a consumer.

set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSCRIPTEN_ROOT EMSDK PATH EM_CONFIG)

if(NOT DEFINED ENV{EMSCRIPTEN_ROOT})
   find_path(EMSCRIPTEN_ROOT "emcc")
else()
   set(EMSCRIPTEN_ROOT "$ENV{EMSCRIPTEN_ROOT}")
endif()

if(NOT EMSCRIPTEN_ROOT)
   if(NOT DEFINED ENV{EMSDK})
      message(FATAL_ERROR "The emcc compiler not found in PATH")
   endif()
   set(EMSCRIPTEN_ROOT "$ENV{EMSDK}/upstream/emscripten")
endif()

if(NOT EXISTS "${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
   message(FATAL_ERROR "Emscripten.cmake toolchain file not found")
endif()

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake")
set(VCPKG_BUILD_TYPE release)

# SIZEOF_SIZE_T is read by one of the dependencies during configure and is not inferred on this
# target. The three -m flags are wasm features, not ABI choices.
set(_configureFlags "-msimd128 -mnontrapping-fptoint -mbulk-memory -DSIZEOF_SIZE_T=4")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_C_FLAGS=${_configureFlags}
    -DCMAKE_CXX_FLAGS=${_configureFlags}
    -DCMAKE_EXE_LINKER_FLAGS=${_configureFlags})

# OpenSSL builds a shared-object loader by default, which Emscripten has no use for and cannot
# link. Lifted from cesium-native's own Emscripten CI.
if(PORT MATCHES "openssl")
  set(VCPKG_CONFIGURE_MAKE_OPTIONS "no-dso")
endif()

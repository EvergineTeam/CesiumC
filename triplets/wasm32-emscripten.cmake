# Overlay triplet, taking precedence over vcpkg's community wasm32-emscripten of the same name.
#
# It exists for one reason: the exception and setjmp models have to be the same in every object
# that reaches the link, and CMAKE_CXX_FLAGS cannot say so on vcpkg's behalf. vcpkg builds each
# port in its own configure, governed by the triplet, so the flags this project passes to its
# own build never reached the hundred and eighteen dependencies. They came out using JS
# exceptions and the JS longjmp model while cesium-native and this wrapper used the wasm ones,
# and the link then failed on an undefined emscripten_longjmp coming from a dependency.
#
# The values match what .NET's browser-wasm runtime links with, which is not a guess: its own
# link line passes -mllvm -exception-model=wasm and -lc++-except, and a throwing archive built
# this way links into a .NET wasm application and runs.
#
# -pthread is deliberately absent. It is the one setting a single-threaded host cannot accept,
# which is what CESIUM_WASM_SHARED_MEMORY=OFF turns off in cesium-native.

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)

set(WASM_ABI_FLAGS "-fwasm-exceptions -sSUPPORT_LONGJMP=wasm")
set(VCPKG_C_FLAGS "${WASM_ABI_FLAGS}")
set(VCPKG_CXX_FLAGS "${WASM_ABI_FLAGS}")
set(VCPKG_LINKER_FLAGS "${WASM_ABI_FLAGS}")

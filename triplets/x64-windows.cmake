# Overlay triplet: standard x64-windows but Release-only.
# Overrides the built-in x64-windows so vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)

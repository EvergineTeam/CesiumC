# Overlay triplet: standard arm64-osx but Release-only.
# Overrides the community arm64-osx so vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_BUILD_TYPE release)

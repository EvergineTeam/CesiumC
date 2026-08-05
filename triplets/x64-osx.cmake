# Overlay triplet: standard x64-osx but Release-only.
# Overrides the community x64-osx so vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
#
# Built on an arm64 runner. Apple's clang targets both architectures from either host, so this
# is an ordinary cross-compile rather than anything exotic -- but it does mean the test
# executable cannot run where it was built, which is why the x64 leg skips tests.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)
set(VCPKG_BUILD_TYPE release)

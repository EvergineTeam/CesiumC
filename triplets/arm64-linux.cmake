# Overlay triplet: standard arm64-linux but Release-only.
# Overrides the built-in arm64-linux so vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
#
# Built natively on ubuntu-24.04-arm rather than cross-compiled from x64. Cross-compiling would
# mean an aarch64 toolchain for all thirty dependencies; a native runner needs none, and the
# tests can actually run.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

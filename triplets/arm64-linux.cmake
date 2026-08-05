# Overlay triplet: standard arm64-linux but Release-only.
# Overrides the built-in arm64-linux so vcpkg skips the Debug build of every
# dependency, roughly halving dependency compile time in CI.
#
# Built natively on ubuntu-24.04-arm rather than cross-compiled from x64. Cross-compiling would
# mean an aarch64 toolchain for all thirty dependencies; a native runner needs none, and the
# tests can actually run.
set(VCPKG_TARGET_ARCHITECTURE arm64)
# Without this vcpkg falls back to host semantics and tries to open Visual Studio's
# developer prompt, which on Linux fails with "Use of Visual Studio's Developer Prompt is
# unsupported on non-Windows hosts". The Windows overlay triplets here do not set it
# because their host and target agree; the Darwin one does.
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

cmake_minimum_required(VERSION 3.16)

project(toki
	VERSION 1.3.0
	DESCRIPTION "FIGlet clock, in a terminal and in a window"
	LANGUAGES C CXX)

# CXX is enabled for the project even though src/ is C: minifb keeps one C++
# translation unit, and linking against it has to go through the C++ driver.

# minifb builds its examples and interactive tests unless told otherwise. These
# are option()s, so they have to be set before add_subdirectory picks them up.
set(MINIFB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIFB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIFB_BUILD_VERSION_INFO OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/minifb)

# chaitea ships no CMakeLists.txt. Compiling its sources here instead of running
# its Makefile keeps the whole build in one dependency graph. The source list
# mirrors the OBJS in vendor/chaitea/Makefile; the flags mirror its CFLAGS.
add_library(chai STATIC
	vendor/chaitea/src/allocator.c
	vendor/chaitea/src/chaitea.c
	vendor/chaitea/src/ebuf.c
	vendor/chaitea/src/str.c
	vendor/chaitea/src/time.c
	vendor/chaitea/src/utf8.c
	vendor/chaitea/src/view.c
	vendor/chaitea/src/view_render.c)
add_library(chai::chai ALIAS chai)

target_include_directories(chai PUBLIC vendor/chaitea/src)
target_compile_features(chai PUBLIC c_std_11)
# gnu11, not c11: chaitea's own Makefile builds it with -std=gnu11.
set_target_properties(chai PROPERTIES C_EXTENSIONS ON)
target_compile_options(chai PRIVATE -Wall -Wextra -Wpedantic)

# chaitea does not build out of the box on macOS, and its own Makefile does not
# paper over either of these:
#   - terminal.h #errors out unless _POSIX_C_SOURCE >= 199309L, and it is a
#     public header, so consumers inherit the requirement.
#   - chaitea.c includes <threads.h>, which Apple's libc does not ship.
#     vendor_win carries the c11threads portability header; only chaitea.c
#     includes it, so the path stays private.
# Both match what the hand-written Makefile passed to chaitea's build.
target_compile_definitions(chai PUBLIC _POSIX_C_SOURCE=200809L)
target_include_directories(chai PRIVATE vendor/chaitea/vendor_win)

# The windowed clock. minifb::minifb carries its own include directories and,
# being a static library, propagates the macOS frameworks it needs.
add_executable(toki
	src/toki.c
	src/Config.c
	src/FrameBuffer.c
	src/Glyphs.c
	src/KittyRender.c)
target_compile_features(toki PRIVATE c_std_11)
set_target_properties(toki PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(toki PRIVATE minifb::minifb)
# Every minifb backend calls release_cpp_stub() when a window is torn down, and
# that symbol is defined only in MiniFB_cpp.cpp. The C++ driver is what pulls in
# libc++ for it; linking as C fails with undefined C++ runtime symbols.
set_target_properties(toki PROPERTIES LINKER_LANGUAGE CXX)

# The original terminal clock, still built against chaitea.
add_executable(toki-term
	src/toki_term.c)
target_compile_features(toki-term PRIVATE c_std_11)
set_target_properties(toki-term PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(toki-term PRIVATE chai::chai)

# The warnings the Makefile has always used for src/. Vendored code keeps its
# own flags, so these stay off the chai and minifb targets.
target_compile_options(toki PRIVATE -Wall -Wextra)
target_compile_options(toki-term PRIVATE -Wall -Wextra)

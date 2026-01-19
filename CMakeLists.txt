cmake_minimum_required(VERSION 3.10)

project(ReRecycleBin)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_C_COMPILER cl)
# set(CMAKE_CXX_COMPILER cl)
# set(CMAKE_C_COMPILER clang-cl)
# set(CMAKE_CXX_COMPILER clang-cl)
# set(CMAKE_C_COMPILER "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe")
# set(CMAKE_CXX_COMPILER "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe")
set(CMAKE_C_COMPILER "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/HostX64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/HostX64/x64/cl.exe")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(_DEBUG_MODE ON)

# set(CMAKE_LINKER "C:/Users/AUTO/Downloads/raddbg/radlink.exe" CACHE FILEPATH "Radlink linker")

if(MSVC)
    # add_compile_options(-std=c++latest -std:c++latest /std:c++latest /permissive- /Zc:__cplusplus)
    # add_compile_options(/arch:AVX2)
    # add_compile_options(/openmp:llvm /fsanitize=address)
    add_compile_options(/openmp:llvm)
endif()

# OpenSSL
# set(OPENSSL_ROOT_DIR ${PROJECT_SOURCE_DIR}/vendor/clamwin_openssl)
# set(OPENSSL_CRYPTO_LIBRARY ${PROJECT_SOURCE_DIR}/vendor/clamwin_openssl/lib/msvc/x64/libcrypto.lib)
# set(OPENSSL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/vendor/OpenSSL-Win64/include)
# set(OPENSSL_LIBRARIES ${PROJECT_SOURCE_DIR}/vendor/OpenSSL-Win64/lib/VC)
find_package(OpenSSL REQUIRED)

# threadpool
set(TP_BUILD_TESTS OFF)
set(TP_BUILD_EXAMPLES OFF)
set(TP_BUILD_BENCHMARKS OFF)
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/threadpool)

# threadpool
add_library(BS_thread_pool INTERFACE)
target_include_directories(BS_thread_pool INTERFACE ${PROJECT_SOURCE_DIR}/vendor/threadpool2/include)

# argparse
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/argparse)

# 
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/spdlog)

# sccache
# find_program(SCCACHE sccache REQUIRED)
# set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE})
# set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE})
# set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
# cmake_policy(SET CMP0141 NEW)

#OpenGL
# find_package(OpenGL REQUIRED)

# SDL2
# set(SDL_TESTS OFF)
# add_subdirectory(lib/SDL2)

# microui
# file(GLOB MICROUI_SOURCES lib/microui/src/*.c)
# file(GLOB MICROUI_HEADERS lib/microui/src/*.h)
# add_library(libmicroui ${MICROUI_SOURCES})
# target_sources(libmicroui INTERFACE ${MICROUI_HEADERS})
# target_include_directories(libmicroui PUBLIC ${PROJECT_SOURCE_DIR}/lib/microui/src)

# #IMGUI
# file(GLOB IMGUI_SOURCES vendor/imgui/*.cpp)
# file(GLOB IMGUI_HEADERS vendor/imgui/*.h)
# file(GLOB IMGUI_BACKEND_SOURCES vendor/imgui/backends/imgui_impl_dx11.cpp vendor/imgui/backends/imgui_impl_win32.cpp)
# file(GLOB IMGUI_BACKEND_HEADERS vendor/imgui/backends/imgui_impl_dx11.h vendor/imgui/backends/imgui_impl_win32.h)
# add_library(libimgui ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES})
# target_sources(libimgui INTERFACE ${IMGUI_HEADERS} ${IMGUI_BACKEND_HEADERS})
# target_include_directories(libimgui PUBLIC ${PROJECT_SOURCE_DIR}/vendor/imgui PUBLIC ${PROJECT_SOURCE_DIR}/vendor/imgui/backends)

# #blake3
# set(BLAKE3_SIMD_TYPE "x86-intrinsics" CACHE STRING "Use compiler intrinsics instead of MASM")
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/blake3/c)

# #cppcoro
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/cppcoro)

#sqlite
set(SQLITE_DEFAULT_FOREIGN_KEYS ON)
# add_definitions(-DSQLITE_THREADSAFE=2)
file(GLOB SQLITE_SOURCES ${PROJECT_SOURCE_DIR}/vendor/sqlite/*.c)
file(GLOB SQLITE_HEADERS ${PROJECT_SOURCE_DIR}/vendor/sqlite/*.h)
add_library(sqlite ${SQLITE_SOURCES})
target_sources(sqlite INTERFACE ${SQLITE_HEADERS})
target_include_directories(sqlite PUBLIC ${PROJECT_SOURCE_DIR}/vendor/sqlite)

# re2
file(GLOB_RECURSE LIB_SOURCES lib/*.cpp lib/**/*.cpp)
file(GLOB_RECURSE LIB_HEADERS lib/*.hpp lib/**/*.hpp)
add_library(lib ${LIB_SOURCES})
target_sources(lib INTERFACE ${LIB_HEADERS})
target_include_directories(lib PUBLIC ${PROJECT_SOURCE_DIR}/lib)

# all link
target_link_libraries(lib OpenSSL::SSL libcppcoro BLAKE3::blake3 sqlite dp::thread-pool argparse::argparse spdlog::spdlog BS_thread_pool)
# target_compile_definitions(lib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
link_libraries(lib)

# if(MSVC)
#     add_compile_options(/W0)  # Enable warning level 4 for MSVC
#     target_compile_options(re2 PUBLIC /std:c++latest)
#     target_compile_options(re2 PUBLIC /permissive- /Zc:__cplusplus)
# endif()

# target_link_libraries(re2 PRIVATE
#     librere
#     libmicroui
#     SDL2::SDL2
#     SDL2::SDL2main
# )

add_executable(re2 src/main.cpp)

file(GLOB SOURCE_FILES RELATIVE ${PROJECT_SOURCE_DIR}/test test/*.cpp)
foreach (BIN_SOURCE ${SOURCE_FILES})
    string(REPLACE ".cpp" "" BIN ${BIN_SOURCE})
    target_compile_definitions(lib PRIVATE DEBUG)
    add_executable(${BIN} test/${BIN_SOURCE})
    target_link_libraries(${BIN} lib)
endforeach ()
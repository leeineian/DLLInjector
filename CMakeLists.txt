cmake_minimum_required(VERSION 3.20)
project(dllInjector VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Unicode compiling definitions
add_compile_definitions(UNICODE _UNICODE)

add_executable(dllInjector WIN32)
add_subdirectory(src)

# Set output name to DLLInjector
set_target_properties(dllInjector PROPERTIES OUTPUT_NAME "DLLInjector")

# Force MSVC to generate manifest with requireAdministrator to match our custom manifest
if(MSVC)
    set_target_properties(dllInjector PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

# Link native Windows libraries
target_link_libraries(dllInjector PRIVATE
    comctl32
    comdlg32
    shell32
    advapi32
    shlwapi
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

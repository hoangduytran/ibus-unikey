cmake_minimum_required (VERSION 3.5)
project (ibus-unikey)

enable_testing()

cmake_policy(SET CMP0054 NEW)

# compile_commands.json in the build tree (e.g. build/compile_commands.json) for clangd / IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-std=c++11 -Werror)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(GNUInstallDirs)

set(LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}" CACHE string "where to install ibus-{engine,setup}-unikey")
set(PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
set(LOCALEDIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -L ${LIBEXECDIR}")

add_definitions(
    -DPACKAGE_NAME="${PROJECT_NAME}"
    -DGETTEXT_PACKAGE="${PROJECT_NAME}"
    -DPACKAGE_VERSION="0.7.0"
    -DPACKAGE_BUGREPORT="https://github.com/vn-input/ibus-unikey/issues"
    -DLOCALEDIR="${LOCALEDIR}"
    -DPKGDATADIR="${PKGDATADIR}"
    -DLIBEXECDIR="${LIBEXECDIR}"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include/ukengine
    ${PROJECT_SOURCE_DIR}/include/ukengine/mapping
    ${PROJECT_SOURCE_DIR}/include/ukengine/tables
    ${PROJECT_SOURCE_DIR}/include/ukengine/patterns
    ${PROJECT_SOURCE_DIR}/include/ukengine/roof
    ${PROJECT_SOURCE_DIR}/include/ukengine/core
    ${PROJECT_SOURCE_DIR}/include/ukengine/engine
    ${PROJECT_SOURCE_DIR}/src/config
)

add_subdirectory(ukengine)
add_subdirectory(src)
add_subdirectory(setup)
add_subdirectory(data)
add_subdirectory(po)
add_subdirectory(tests)

# With enable_testing(), CMake generates a `test` target (e.g. `make test` / `cmake --build . --target test`)
# that runs CTest. Do not add_custom_target(test) — that name is reserved.


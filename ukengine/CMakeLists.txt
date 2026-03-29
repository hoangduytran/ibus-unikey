set(SOURCES
    core/byteio.cpp
    core/inputproc.cpp
    mapping/charset.cpp
    mapping/mactab.cpp
    tables/data.cpp
    tables/error.cpp
    tables/usrkeymap.cpp
    patterns/pattern.cpp
    roof/convert.cpp
    engine/ukengine.cpp
    engine/unikey.cpp
)

add_library(ukengine STATIC ${SOURCES})

target_include_directories(ukengine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/mapping
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/tables
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/patterns
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/roof
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/engine
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mapping
    ${CMAKE_CURRENT_SOURCE_DIR}/tables
    ${CMAKE_CURRENT_SOURCE_DIR}/patterns
    ${CMAKE_CURRENT_SOURCE_DIR}/roof
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
)

set_target_properties(ukengine PROPERTIES COMPILE_FLAGS "-funsigned-char -Wno-invalid-source-encoding")


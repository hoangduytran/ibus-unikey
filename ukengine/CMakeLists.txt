set(SOURCES
    core/byteio.cpp
    core/inputproc.cpp
    mapping/charset.cpp
    mapping/mactab.cpp
    tables/singlebyte/tcvn3.cpp
    tables/singlebyte/vps.cpp
    tables/singlebyte/viscii.cpp
    tables/singlebyte/bkhcm1.cpp
    tables/singlebyte/vietware_f.cpp
    tables/singlebyte/isc.cpp
    tables/doublebytes/vni_win.cpp
    tables/doublebytes/bkhcm2.cpp
    tables/doublebytes/vietware_x.cpp
    tables/doublebytes/vni_mac.cpp
    tables/data.cpp
    tables/error.cpp
    tables/usrkeymap.cpp
    patterns/pattern.cpp
    roof/convert.cpp
    engine/ukengine.cpp
    engine/unikey.cpp
)

add_library(ukengine STATIC ${SOURCES})

target_include_directories(ukengine
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/mapping
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/tables
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/patterns
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/roof
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/core
      ${CMAKE_CURRENT_SOURCE_DIR}/../include/ukengine/engine
      ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(ukengine PROPERTIES COMPILE_FLAGS "-funsigned-char -Wno-invalid-source-encoding -Wno-narrowing")

# Dump each SingleByteTables[] row (same bytes the compiler used for data.cpp) to .hex files for regenerating table sources.
add_executable(dump_singlebyte_hex
    ${CMAKE_SOURCE_DIR}/tools/dump_singlebyte_hex.cpp
)
target_link_libraries(dump_singlebyte_hex PRIVATE ukengine)
set_target_properties(dump_singlebyte_hex PROPERTIES
    COMPILE_FLAGS "-funsigned-char -Wno-invalid-source-encoding -Wno-narrowing"
)


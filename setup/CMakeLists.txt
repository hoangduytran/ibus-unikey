find_package(PkgConfig)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

add_library(macro_interchange STATIC
    macro_interchange_common.cpp
    macro_format_handler_registry.cpp
    macro_handler_text.cpp
    macro_handler_yaml.cpp
    macro_handler_plist.cpp
    macro_handler_json.cpp
    macro_handler_csv.cpp
    macro_file_io.cpp
)

target_include_directories(macro_interchange PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/setup
    ${GTK3_INCLUDE_DIRS}
)

target_link_libraries(macro_interchange PUBLIC
    ukengine
    ${GTK3_LIBRARIES}
)

add_executable(ibus-setup-unikey
    main.cpp
    signal_handlers.cpp
    macro_utils.cpp
    ui/setup_view.cpp
    config/settings_store.cpp
    controller/setup_controller.cpp
)

target_include_directories(ibus-setup-unikey PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/controller
)
# Pull every object from `macro_interchange` so per-handler static registrars run
# (otherwise the linker may drop .o files whose only entry points are constructors).
if(APPLE)
  target_link_libraries(ibus-setup-unikey PRIVATE
      "-Wl,-force_load,$<TARGET_FILE:macro_interchange>"
      ukengine
      unikey-config
      ${GTK3_LIBRARIES}
  )
else()
  target_link_libraries(ibus-setup-unikey PRIVATE
      -Wl,--whole-archive
      macro_interchange
      -Wl,--no-whole-archive
      ukengine
      unikey-config
      ${GTK3_LIBRARIES}
  )
endif()
target_include_directories(ibus-setup-unikey PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_options(ibus-setup-unikey PRIVATE -rdynamic)

install(TARGETS ibus-setup-unikey DESTINATION ${LIBEXECDIR})
install(FILES
    ui/main_window.ui
    ui/macro_dialog.ui
    DESTINATION ${PKGDATADIR}/ui
)

find_package(PkgConfig)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

add_executable(ibus-setup-unikey
    main.cpp
    signal_handlers.cpp
    macro_utils.cpp
    ui/setup_view.cpp
    config/settings_store.cpp
    controller/setup_controller.cpp
)

target_include_directories(ibus-setup-unikey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/controller
)
target_link_libraries(ibus-setup-unikey
    ukengine
    unikey-config
    ${GTK3_LIBRARIES}
)
target_include_directories(ibus-setup-unikey PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_options(ibus-setup-unikey PRIVATE -rdynamic)

install(TARGETS ibus-setup-unikey DESTINATION ${LIBEXECDIR})
install(FILES ibus-unikey.ui DESTINATION ${PKGDATADIR}/ui)


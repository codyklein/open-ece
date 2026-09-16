find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
find_package(Qwt 6.2 REQUIRED)

# Internal GUI library allows integration tests to exercise the same workbench as main().
add_library(openece_workbench STATIC main_window.cpp signals_dsp_view.cpp plot_widget.cpp phase_input.cpp)
target_include_directories(openece_workbench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openece_workbench
    PUBLIC Qt6::Widgets Qwt::Qwt
    PRIVATE OpenECE::signals OpenECE::dsp openece_options)
target_compile_features(openece_workbench PUBLIC cxx_std_20)
# Keep Qt's optional `signals` macro from colliding with our domain namespace.
target_compile_definitions(openece_workbench PUBLIC QT_NO_KEYWORDS)

qt_add_executable(openece main.cpp)
set_target_properties(openece PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(openece PRIVATE openece_workbench openece_options)

target_compile_definitions(openece PRIVATE OPENECE_VERSION="${PROJECT_VERSION}")
if(WIN32)
    # The package script consumes the exact Release artifact chosen by FindQwt.
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/qwt-runtime-$<CONFIG>.txt"
        CONTENT "$<TARGET_FILE:Qwt::Qwt>")
    install(TARGETS openece RUNTIME DESTINATION . CONFIGURATIONS Release)
    install(FILES "$<TARGET_FILE:Qwt::Qwt>" DESTINATION . CONFIGURATIONS Release)
    # Fallback for Qt versions that deploy a VC redist installer rather than DLLs.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        DESTINATION . CONFIGURATIONS Release COMPONENT CompilerRuntimeFallback)
endif()

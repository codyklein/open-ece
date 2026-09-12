find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Qwt REQUIRED IMPORTED_TARGET Qt6Qwt6>=6.2)

# Internal GUI library allows integration tests to exercise the same workbench as main().
add_library(openece_workbench STATIC main_window.cpp plot_widget.cpp)
target_include_directories(openece_workbench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openece_workbench
    PUBLIC Qt6::Widgets PkgConfig::Qwt
    PRIVATE OpenECE::signals OpenECE::dsp openece_options)
target_compile_features(openece_workbench PUBLIC cxx_std_20)
# Keep Qt's optional `signals` macro from colliding with our domain namespace.
target_compile_definitions(openece_workbench PUBLIC QT_NO_KEYWORDS)

add_executable(openece main.cpp)
target_link_libraries(openece PRIVATE openece_workbench openece_options)

# Internal presentation data only: deliberately configured outside OPENECE_BUILD_GUI.
add_library(openece_schematic_model STATIC projection.cpp)
target_include_directories(openece_schematic_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(openece_schematic_model PUBLIC cxx_std_20)
target_link_libraries(openece_schematic_model PRIVATE OpenECE::project openece_options)

add_library(openece_signals STATIC src/sine.cpp)
add_library(OpenECE::signals ALIAS openece_signals)
target_include_directories(openece_signals PUBLIC include)
target_link_libraries(openece_signals PUBLIC OpenECE::core PRIVATE openece_options)

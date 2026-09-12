add_library(openece_dsp STATIC src/fft.cpp src/window.cpp)
add_library(OpenECE::dsp ALIAS openece_dsp)
target_include_directories(openece_dsp PUBLIC include)
target_link_libraries(openece_dsp PUBLIC OpenECE::core PRIVATE openece_options)

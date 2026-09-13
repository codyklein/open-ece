# Fedora keeps its system package workflow. An explicit QWT_ROOT selects a
# Qt-6 shared Qwt installation (the Windows bootstrap supplies its artifact map).
if(NOT QWT_ROOT AND NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Qwt REQUIRED IMPORTED_TARGET Qt6Qwt6>=${Qwt_FIND_VERSION})
    add_library(Qwt::Qwt INTERFACE IMPORTED)
    set_target_properties(Qwt::Qwt PROPERTIES
        INTERFACE_LINK_LIBRARIES PkgConfig::Qwt)
    return()
endif()

find_path(Qwt_INCLUDE_DIR qwt.h PATHS "${QWT_ROOT}/include" NO_DEFAULT_PATH)
if(EXISTS "${QWT_ROOT}/OpenECEQwtArtifacts.cmake")
    include("${QWT_ROOT}/OpenECEQwtArtifacts.cmake")
else()
    # Alternative installations may supply these cache entries explicitly.
    find_library(Qwt_LIBRARY_RELEASE NAMES qwt PATHS "${QWT_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(Qwt_LIBRARY_DEBUG NAMES qwtd PATHS "${QWT_ROOT}/lib" NO_DEFAULT_PATH)
    find_file(Qwt_RUNTIME_RELEASE NAMES qwt.dll PATHS "${QWT_ROOT}/lib" "${QWT_ROOT}/bin" NO_DEFAULT_PATH)
    find_file(Qwt_RUNTIME_DEBUG NAMES qwtd.dll PATHS "${QWT_ROOT}/lib" "${QWT_ROOT}/bin" NO_DEFAULT_PATH)
endif()
if(Qwt_INCLUDE_DIR)
    file(STRINGS "${Qwt_INCLUDE_DIR}/qwt_global.h" _qwt_version_line REGEX "^#define QWT_VERSION_STR")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" Qwt_VERSION "${_qwt_version_line}")
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qwt REQUIRED_VARS Qwt_INCLUDE_DIR
    Qwt_LIBRARY_RELEASE Qwt_LIBRARY_DEBUG Qwt_RUNTIME_RELEASE Qwt_RUNTIME_DEBUG
    VERSION_VAR Qwt_VERSION)
if(Qwt_FOUND)
    add_library(Qwt::Qwt SHARED IMPORTED)
    set_target_properties(Qwt::Qwt PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release"
        IMPORTED_IMPLIB_RELEASE "${Qwt_LIBRARY_RELEASE}"
        IMPORTED_IMPLIB_DEBUG "${Qwt_LIBRARY_DEBUG}"
        IMPORTED_LOCATION_RELEASE "${Qwt_RUNTIME_RELEASE}"
        IMPORTED_LOCATION_DEBUG "${Qwt_RUNTIME_DEBUG}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
        INTERFACE_INCLUDE_DIRECTORIES "${Qwt_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS QWT_DLL
        INTERFACE_LINK_LIBRARIES Qt6::Widgets)
endif()

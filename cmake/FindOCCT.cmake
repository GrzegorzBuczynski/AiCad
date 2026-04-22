# Find Open CASCADE (OCCT) via vcpkg or system installation.

# Try vcpkg first - find the opencascade subdirectory
find_path(OCCT_OPENCASCADE_INCLUDE_DIR
    NAMES gp_Dir.hxx
    PATHS
        "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include/opencascade"
    NO_DEFAULT_PATH
)

find_library(OCCT_TKERNEL_LIB
    NAMES TKernel
    PATHS
        "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib"
        "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/lib"
    NO_DEFAULT_PATH
)

# If not found via vcpkg, try system paths
if(NOT OCCT_OPENCASCADE_INCLUDE_DIR)
    find_path(OCCT_OPENCASCADE_INCLUDE_DIR
        NAMES gp_Dir.hxx
        PATH_SUFFIXES opencascade
    )
endif()

if(NOT OCCT_TKERNEL_LIB)
    find_library(OCCT_TKERNEL_LIB
        NAMES TKernel
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OCCT
    REQUIRED_VARS OCCT_OPENCASCADE_INCLUDE_DIR OCCT_TKERNEL_LIB
)

if(OCCT_FOUND)
    # The include directory should be the opencascade/ directory itself
    # so that #include <gp_Dir.hxx> resolves correctly
    set(OCCT_INCLUDE_DIRS "${OCCT_OPENCASCADE_INCLUDE_DIR}")

    # Collect all OCCT libraries
    set(OCCT_LIB_NAMES
        TKMath TKernel TKService TKV3d TKOpenGl TKTopAlgo TKMesh TKPrim
        TKBO TKBRep TKGeomBase TKG3d TKG2d TKGeomAlgo TKShHealing
        TKHLR TKFillet TKOffset TKFeat TKBool PTKernel TKLCAF TKCAF
        TKBin TKXml TKBinL TKXmlL TKBinXCAF TKXmlXCAF TKXCAF TKXDESTEP
        TKXDEIGES TKSTL TKVRML TKCDF
    )

    set(OCCT_LIBRARIES "")
    foreach(lib_name ${OCCT_LIB_NAMES})
        find_library(OCCT_${lib_name}_LIB
            NAMES ${lib_name}
            PATHS
                "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib"
                "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/lib"
            NO_DEFAULT_PATH
        )
        if(OCCT_${lib_name}_LIB)
            list(APPEND OCCT_LIBRARIES ${OCCT_${lib_name}_LIB})
        endif()
    endforeach()

    message(STATUS "OCCT found: include=${OCCT_INCLUDE_DIRS}")
    message(STATUS "OCCT libraries: ${OCCT_LIBRARIES}")
endif()

project (LIBVPX)

include (ExternalProject)

cmake_minimum_required (VERSION 2.16)

set (LIBVPX_GIT            "https://github.com/webmproject/libvpx.git" )


option (LIBVPX_FORCE_BUILD    "Force build and installation of package?" YES  )


#	configure.args  --enable-vp8 \
#	                --enable-psnr \
#	                --enable-postproc \
#	                --enable-multithread \
#	                --enable-runtime-cpu-detect \
#	                --disable-install-docs \
#	                --disable-debug-libs \
#	                --disable-examples

set(LIBVPX_PREFIX ${PROJECT_BINARY_DIR}/libvpx)

ExternalProject_Add (vpx_dependency
    PREFIX ${LIBVPX_PREFIX}
    GIT_REPOSITORY ${LIBVPX_GIT}
    SOURCE_DIR ${LIBVPX_PREFIX}/src
    BINARY_DIR ${LIBVPX_PREFIX}/build
    STAMP_DIR ${LIBVPX_PREFIX}/stamp
    GIT_TAG "v1.15.0"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    UPDATE_COMMAND  ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ${LIBVPX_PREFIX}/src/configure --prefix=${CMAKE_INSTALL_PREFIX} --enable-multithread --enable-runtime-cpu-detect
)

add_library(libvpx STATIC IMPORTED)
set_target_properties(libvpx PROPERTIES
    IMPORTED_LOCATION ${LIBVPX_PREFIX}/build/libvpx.a
    INTERFACE_INCLUDE_DIRECTORIES ${LIBVPX_PREFIX}/src
)
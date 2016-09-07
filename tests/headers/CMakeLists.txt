#
# Test that all header files compile stand-alone and that no public header includes an internal one.
#

set(root_inc_dir ${CMAKE_SOURCE_DIR}/include)

set(subdirs
    unity/storage
    unity/storage/provider
    unity/storage/qt/client
    unity/storage/qt/client/internal/local_client
    unity/storage/qt/client/internal/remote_client
)

set(extra_inc_dirs "${Qt5Core_INCLUDE_DIRS}")
set(extra_inc_dirs "${extra_inc_dirs};${Qt5DBus_INCLUDE_DIRS}")
set(extra_inc_dirs "${extra_inc_dirs};${Qt5Network_INCLUDE_DIRS}")
set(extra_inc_dirs "${extra_inc_dirs};${GIO_DEPS_INCLUDE_DIRS}")
set(extra_inc_dirs "${extra_inc_dirs};${ONLINEACCOUNTS_DEPS_INCLUDE_DIRS}")

set(extra_defines "-D BOOST_THREAD_VERSION=4")

# Replace ; with -I as the include directories are separated by ";"
string(REPLACE ";" " -I/" extra_inc_dirs "${extra_inc_dirs}")
set(extra_inc_dirs " -I${extra_inc_dirs}")

foreach(dir ${OTHER_INCLUDE_DIRS})
    set(other_inc_dirs "${other_inc_dirs} -I${dir}")
endforeach()

set(other_inc_dirs "${other_inc_dirs} -I${CMAKE_BINARY_DIR}/include -I${CMAKE_BINARY_DIR}")

foreach(dir ${subdirs})
    string(REPLACE "/" "-" location ${dir})

    set(public_inc_dir ${root_inc_dir}/${dir})
    set(internal_inc_dir ${public_inc_dir}/internal)

    # Test that each public header compiles stand-alone.
    add_test(stand-alone-${location}-headers
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_headers.py
            ${extra_defines}
            ${public_inc_dir} ${CMAKE_CXX_COMPILER} "${CMAKE_CXX_COMPILER_ARG1} -fPIC -fsyntax-only -I${root_inc_dir} -I${public_inc_dir} ${other_inc_dirs} ${CMAKE_CXX_FLAGS} ${extra_inc_dirs}")

    # Test that each internal header compiles stand-alone.
    if (IS_DIRECTORY ${internal_inc_dir})
        add_test(stand-alone-${location}-internal-headers
            ${CMAKE_CURRENT_SOURCE_DIR}/compile_headers.py
                ${extra_defines}
                ${internal_inc_dir} ${CMAKE_CXX_COMPILER} "${CMAKE_CXX_COMPILER_ARG1} -fPIC -fsyntax-only -I${root_inc_dir} -I${internal_inc_dir} ${other_inc_dirs} ${CMAKE_CXX_FLAGS} ${extra_inc_dirs}")
    endif()

    if (NOT ${public_inc_dir} MATCHES "/internal/")
        # Test that no public header includes an internal header
        add_test(clean-public-${location}-headers ${CMAKE_CURRENT_SOURCE_DIR}/check_public_headers.py ${public_inc_dir})
    endif()
endforeach()

# Default install location. Must be set here, before setting the project.
if (NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "" FORCE)
    set(LOCAL_INSTALL "ON")
endif()

cmake_minimum_required(VERSION 3.0.2)
project(storage-framework VERSION "0.2" LANGUAGES C CXX)

# These variables should be incremented when we wish to create a new
# source incompatible version of the library where users of the
# old API will not compile against the new one.  It is not
# necessary to increment this for ABI breaks that are source compatible.
set(SF_CLIENT_API_VERSION "2")
set(SF_PROVIDER_API_VERSION "1")

# These two should be incremented when the ABI changes.
set(SF_CLIENT_SOVERSION "0")
execute_process(
  COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/get-provider-soversion.sh
  OUTPUT_VARIABLE SF_PROVIDER_SOVERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Error running get-provider-soversion.sh script")
endif()

set(SF_CLIENT_LIBVERSION "${SF_CLIENT_SOVERSION}.${PROJECT_VERSION}")
set(SF_PROVIDER_LIBVERSION "${SF_PROVIDER_SOVERSION}.${PROJECT_VERSION}")


string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower) # Build types should always be lower case

set(ACCEPTED_BUILD_TYPES "" none release debug relwithdebinfo coverage)
list(FIND ACCEPTED_BUILD_TYPES "${cmake_build_type_lower}" IS_BUILD_TYPE_ACCEPTED)
if (${IS_BUILD_TYPE_ACCEPTED} EQUAL -1)
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\nValid types are: ${ACCEPTED_BUILD_TYPES}")
endif()

# Ensure that generated files can be linked with.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(include)
include_directories(${CMAKE_BINARY_DIR}/include)  # For generated headers

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC --std=c++11 -Wall -pedantic -Wextra -fvisibility=hidden")

# Some additional warnings not included by the general flags set above.
set(EXTRA_C_WARNINGS "-Wcast-align -Wcast-qual -Wformat -Wredundant-decls -Wswitch-default")
set(EXTRA_CXX_WARNINGS "-Wnon-virtual-dtor -Wctor-dtor-privacy -Wold-style-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_C_WARNINGS} ${EXTRA_CXX_WARNINGS}")

# By default, for release builds, warnings become hard errors.
if ("${cmake_build_type_lower}" STREQUAL "release" OR "${cmake_build_type_lower}" STREQUAL "relwithdebinfo")
    option(Werror "Treat warnings as errors" ON)
else()
    option(Werror "Treat warnings as errors" OFF)
endif()

# If warnings are errors, don't error on deprecated declarations.
if (${Werror})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    if ("${cmake_build_type_lower}" STREQUAL "release" OR "${cmake_build_type_lower}" STREQUAL "relwithdebinfo")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-declarations")
    endif()
endif()

# Flags for address and undefined behavior sanitizer
set(SANITIZER "" CACHE STRING "Build with -fsanitize=<value> (legal values: address, ub)")

if ("${SANITIZER}" STREQUAL "")
    # Do nothing
elseif (${SANITIZER} STREQUAL "ub")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=float-divide-by-zero -fno-omit-frame-pointer -g")
elseif (${SANITIZER} STREQUAL "address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
else()
    message(FATAL_ERROR "Invalid SANITIZER setting: ${SANITIZER}")
endif()

# Some tests are slow, so make it possible not to run them
# during day-to-day development.
option(slowtests "Run slow tests" ON)
if (${slowtests})
    add_definitions(-DSLOW_TESTS=1)
else()
    add_definitions(-DSLOW_TESTS=0)
endif()

# Definitions for testing with valgrind.

configure_file(CTestCustom.cmake.in CTestCustom.cmake) # Tests in CTestCustom.cmake are skipped for valgrind

find_program(MEMORYCHECK_COMMAND NAMES valgrind)
if (MEMORYCHECK_COMMAND)
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--suppressions=${CMAKE_SOURCE_DIR}/valgrind-suppress --errors-for-leak-kinds=definite --show-leak-kinds=definite --leak-check=full --num-callers=50 --error-exitcode=3"
    )
    add_custom_target(valgrind DEPENDS NightlyMemCheck)
else()
    message(WARNING "Cannot find valgrind: valgrind target will not be available")
endif()

include(CTest)
enable_testing()

find_package(CoverageReport)

include(GNUInstallDirs)

find_package(Boost 1.56 COMPONENTS filesystem system thread REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Test REQUIRED)

include(FindPkgConfig)
pkg_check_modules(APPARMOR_DEPS REQUIRED libapparmor)
pkg_check_modules(GLIB_DEPS REQUIRED glib-2.0)
pkg_check_modules(ONLINEACCOUNTS_DEPS REQUIRED OnlineAccountsQt)

add_definitions(-DQT_NO_KEYWORDS)

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(plugins/Ubuntu/StorageFramework)
add_subdirectory(tests)
add_subdirectory(demo)

enable_coverage_report(
    TARGETS
        qt-client-lib-common
        storage-framework-common-internal
        storage-framework-qt-client
        storage-framework-qt-client-v2
        storage-framework-qt-local-client
        sf-provider-objects
        storage-framework-provider
    FILTER
        ${CMAKE_SOURCE_DIR}/tests/*
        ${CMAKE_BINARY_DIR}/*
    TESTS
        ${UNIT_TEST_TARGETS}
)

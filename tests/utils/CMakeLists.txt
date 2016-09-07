pkg_check_modules(QTDBUSTEST_DEPS libqtdbustest-1 REQUIRED)

set_source_files_properties(${CMAKE_SOURCE_DIR}/data/provider.xml PROPERTIES
    CLASSNAME ProviderClient
    INCLUDE unity/storage/internal/dbusmarshal.h
)
qt5_add_dbus_interface(generated_files
    ${CMAKE_SOURCE_DIR}/data/provider.xml
    ProviderClient
)
set_source_files_properties(${generated_files} PROPERTIES GENERATED TRUE)

add_library(testutils STATIC
  DBusEnvironment.cpp
  ProviderFixture.cpp
  ${generated_files}
)
target_compile_options(testutils PRIVATE ${QTDBUSTEST_DEPS_CFLAGS} ${ONLINEACCOUNTS_DEPS_CFLAGS})
target_link_libraries(testutils
  PUBLIC Qt5::DBus Qt5::Core
  PRIVATE Qt5::Test ${QTDBUSTEST_DEPS_LIBRARIES} ${ONLINEAACOUNTS_DEPS_LIBRARIES})

add_definitions(-DBOOST_THREAD_VERSION=4)

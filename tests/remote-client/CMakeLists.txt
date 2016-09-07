add_executable(remote-client_test remote-client_test.cpp MockProvider.cpp)
set_target_properties(remote-client_test PROPERTIES AUTOMOC TRUE)

add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}" -DBOOST_THREAD_VERSION=4)
include_directories(${GLIB_DEPS_INCLUDE_DIRS})

target_link_libraries(remote-client_test
    storage-framework-provider
    storage-framework-qt-client
    Qt5::Network
    Qt5::Test
    ${Boost_LIBRARIES}
    ${GLIB_DEPS_LIBRARIES}
    testutils
    gtest
)
add_test(remote-client remote-client_test)
add_dependencies(remote-client_test qt-client-all-headers provider-test)

set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)

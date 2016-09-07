add_executable(provider-DBusPeerCache_test DBusPeerCache_test.cpp)
target_link_libraries(provider-DBusPeerCache_test
  storage-framework-provider-static
  Qt5::Test
  testutils
  gtest
  )
add_test(provider-DBusPeerCache provider-DBusPeerCache_test)

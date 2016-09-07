add_executable(provider-AccountData_test AccountData_test.cpp)
target_link_libraries(provider-AccountData_test
  storage-framework-provider-static
  Qt5::Test
  testutils
  gtest
  )
add_test(provider-AccountData provider-AccountData_test)

add_executable(provider-ProviderInterface_test
  ProviderInterface_test.cpp
  TestProvider.cpp
  ${generated_files}
)
set_target_properties(provider-ProviderInterface_test PROPERTIES
  AUTOMOC TRUE
)
target_link_libraries(provider-ProviderInterface_test
  storage-framework-common-internal
  storage-framework-provider
  Qt5::Test
  testutils
  gtest
)
add_test(provider-ProviderInterface provider-ProviderInterface_test)

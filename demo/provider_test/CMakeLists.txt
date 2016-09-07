add_definitions(-DBOOST_THREAD_VERSION=4)

add_executable(provider-test provider-test.cpp)
target_link_libraries(provider-test storage-framework-provider)

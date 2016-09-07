set(includeprefix unity/storage/provider)
file(GLOB provider_headers *.h)

install(FILES ${provider_headers}
  DESTINATION ${provider_base_includedir}/${includeprefix})

add_subdirectory(testing)

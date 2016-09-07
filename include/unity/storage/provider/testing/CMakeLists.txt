set(includeprefix unity/storage/provider/testing)
file(GLOB provider_headers *.h)

install(FILES ${provider_headers}
  DESTINATION ${provider_base_includedir}/${includeprefix})

# Create an OBJECT library for the files that are used by both
# local and remote client libraries, so we don't compile them twice.

add_library(qt-client-lib-common OBJECT
    Account.cpp
    Downloader.cpp
    Exceptions.cpp
    File.cpp
    Folder.cpp
    Item.cpp
    Root.cpp
    Runtime.cpp
    Uploader.cpp
    internal/AccountBase.cpp
    internal/DownloaderBase.cpp
    internal/FileBase.cpp
    internal/FolderBase.cpp
    internal/ItemBase.cpp
    internal/RootBase.cpp
    internal/RuntimeBase.cpp
    internal/UploaderBase.cpp
    ${CMAKE_SOURCE_DIR}/include/unity/storage/qt/client/internal/DownloaderBase.h
    ${CMAKE_SOURCE_DIR}/include/unity/storage/qt/client/internal/UploaderBase.h
)
target_include_directories(qt-client-lib-common PRIVATE
    ${Qt5DBus_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIRS}
)
set_target_properties(qt-client-lib-common PROPERTIES
    AUTOMOC TRUE
    POSITION_INDEPENDENT_CODE TRUE
)

# We build both local and remote client libraries here instead
# of creating OBJECT libraries for them because cmake 3.0.2 (which
# is in the Vivid overlay) cannot handle .moc files in OBJECT libraries.

# Descend into the child directories first. The CMakeLists.txt files
# there set QT_CLIENT_LIB_LOCAL_SRC and QT_CLIENT_LIB_REMOTE_SRC.
add_subdirectory(internal)

# Build the local loopback client library
add_library(storage-framework-qt-local-client SHARED
    $<TARGET_OBJECTS:qt-client-lib-common>
    ${QT_CLIENT_LIB_LOCAL_SRC}
)
target_include_directories(storage-framework-qt-local-client PRIVATE
    ${GLIB_DEPS_INCLUDE_DIRS}
)
set_target_properties(storage-framework-qt-local-client PROPERTIES
    AUTOMOC TRUE
    LINK_FLAGS "-Wl,--no-undefined"
    OUTPUT_NAME "storage-framework-qt-local-client-${SF_CLIENT_API_VERSION}"
    SOVERSION ${SF_CLIENT_SOVERSION}
    VERSION ${SF_CLIENT_LIBVERSION}
)
target_link_libraries(storage-framework-qt-local-client
    storage-framework-common-internal
    Qt5::Concurrent
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    ${Boost_LIBRARIES}
    ${GLIB_DEPS_LIBRARIES}
)
install(
    TARGETS storage-framework-qt-local-client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

configure_file(
    storage-framework-qt-local-client.pc.in
    storage-framework-qt-local-client-${SF_CLIENT_API_VERSION}.pc
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/storage-framework-qt-local-client-${SF_CLIENT_API_VERSION}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Build the remote client library
set_source_files_properties(${CMAKE_SOURCE_DIR}/data/provider.xml PROPERTIES
    CLASSNAME ProviderInterface
    INCLUDE unity/storage/internal/dbusmarshal.h
)

qt5_add_dbus_interface(generated_files
    ${CMAKE_SOURCE_DIR}/data/provider.xml
    ProviderInterface
)
set_source_files_properties(${generated_files} PROPERTIES
    COMPILE_FLAGS "-Wno-ctor-dtor-privacy -Wmissing-field-initializers"
    GENERATED TRUE
)

add_library(storage-framework-qt-client SHARED
    $<TARGET_OBJECTS:qt-client-lib-common>
    ${QT_CLIENT_LIB_REMOTE_SRC}
    ${generated_files}
)
target_include_directories(storage-framework-qt-client PRIVATE
    ${GLIB_DEPS_INCLUDE_DIRS}
    ${ONLINEACCOUNTS_DEPS_INCLUDE_DIRS}
)
set_target_properties(storage-framework-qt-client PROPERTIES
    AUTOMOC TRUE
    LINK_FLAGS "-Wl,--no-undefined"
    OUTPUT_NAME "storage-framework-qt-client-${SF_CLIENT_API_VERSION}"
    SOVERSION ${SF_CLIENT_SOVERSION}
    VERSION ${SF_CLIENT_LIBVERSION}
)
target_link_libraries(storage-framework-qt-client
    storage-framework-common-internal
    Qt5::Concurrent
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    ${ONLINEACCOUNTS_DEPS_LDFLAGS}
)
install(
    TARGETS storage-framework-qt-client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

configure_file(
    storage-framework-qt-client.pc.in
    storage-framework-qt-client-${SF_CLIENT_API_VERSION}.pc
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/storage-framework-qt-client-${SF_CLIENT_API_VERSION}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

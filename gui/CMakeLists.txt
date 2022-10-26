
include_directories(${INCLUDE_DIRS})

root_generate_dictionary(G__TRestDAQGUI TRestDAQGUI.h LINKDEF TRestDAQGUILinkDef.h)
root_generate_dictionary(G__TRestDAQGUIMetadata TRestDAQGUIMetadata.h LINKDEF TRestDAQGUIMetadataLinkDef.h)

add_library(RestDAQGUI SHARED TRestDAQGUIMetadata.cxx TRestDAQGUI.cxx G__TRestDAQGUI.cxx G__TRestDAQGUIMetadata.cxx)

target_include_directories(RestDAQGUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${ROOT_INCLUDE_DIRS} ${REST_INCLUDE_DIRS})

target_link_libraries(RestDAQGUI PUBLIC RESTDAQ ${LINK_LIBRARIES} RestRaw)

install(TARGETS RestDAQGUI DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libTRestDAQGUI_rdict.pcm DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libTRestDAQGUIMetadata_rdict.pcm DESTINATION lib)

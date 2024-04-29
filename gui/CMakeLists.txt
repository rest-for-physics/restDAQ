#set( LibraryVersion "1.0" )
#add_definitions(-DLIBRARY_VERSION="${LibraryVersion}")

#COMPILELIB("")

include_directories(${incdir})

find_package(ROOT 6.24 CONFIG REQUIRED)
root_generate_dictionary(G__TRestDAQGUI TRestDAQGUI.h LINKDEF TRestDAQGUILinkDef.h)
root_generate_dictionary(G__TRestDAQGUIMetadata TRestDAQGUIMetadata.h LINKDEF TRestDAQGUIMetadataLinkDef.h)

add_library(RestDAQGUI SHARED TRestDAQGUIMetadata.cxx TRestDAQGUI.cxx G__TRestDAQGUI.cxx G__TRestDAQGUIMetadata.cxx)

target_include_directories(RestDAQGUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${rest_include_dirs})

target_link_libraries(RestDAQGUI RestDAQ ${lnklib} -lpthread RestRaw)

install(TARGETS RestDAQGUI DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libTRestDAQGUI_rdict.pcm DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libTRestDAQGUIMetadata_rdict.pcm DESTINATION lib)

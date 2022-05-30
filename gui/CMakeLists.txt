#set( LibraryVersion "1.0" )
#add_definitions(-DLIBRARY_VERSION="${LibraryVersion}")

#COMPILELIB("")

include_directories(${incdir})

find_package(ROOT 6.24 CONFIG REQUIRED)
root_generate_dictionary(G__TRestDAQGUI TRestDAQGUI.h LINKDEF TRestDAQGUILinkDef.h)

add_library(RestDAQGUI SHARED TRestDAQGUI.cxx G__TRestDAQGUI.cxx)

target_include_directories(RestDAQGUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${rest_include_dirs})

target_link_libraries(RestDAQGUI RESTDAQ ${lnklib} -lpthread RestRaw)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION .
        COMPONENT install
        )

install(TARGETS RestDAQGUI DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libTRestDAQGUI_rdict.pcm DESTINATION lib)

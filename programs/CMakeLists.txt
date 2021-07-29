include_directories(${incdir})

add_executable(restDAQManager restDAQManager.cxx)
target_link_libraries (restDAQManager LINK_PUBLIC RESTDAQ ${lnklib} )


install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DESTINATION .
         COMPONENT install
                 )

install(TARGETS restDAQManager DESTINATION bin)

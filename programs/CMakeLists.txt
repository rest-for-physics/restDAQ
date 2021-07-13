
add_executable(DCCDAQ DCCDAQ.cxx)
target_link_libraries ( DCCDAQ LINK_PUBLIC TRESTDAQ )

add_executable(clientDCC clientDCC.cxx)
target_link_libraries ( clientDCC LINK_PUBLIC TRESTDAQ )

install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         DESTINATION .
         COMPONENT install
                 )

install(TARGETS DCCDAQ DESTINATION bin)
install(TARGETS clientDCC DESTINATION bin)

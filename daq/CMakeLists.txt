
add_library(RESTDAQ SHARED TRESTDAQ.cxx TRESTDAQSocket.cxx DCCPacket.cxx TRESTDAQDCC.cxx FEMINOSPacket.cxx TRESTDAQFEMINOS.cxx TRESTDAQDummy.cxx TRESTDAQManager.cxx)

target_include_directories(RESTDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ ${rest_include_dirs})

target_link_libraries(RESTDAQ ${lnklib} -lCore RestRaw)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION .
        COMPONENT install
        )

install(TARGETS RESTDAQ DESTINATION lib)


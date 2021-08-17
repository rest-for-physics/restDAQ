
add_library(RESTDAQ SHARED TRESTDAQ/TRESTDAQ.cxx TRESTDAQ/DCCPacket.cxx TRESTDAQ/TRESTDAQDCC.cxx TRESTDAQ/TRESTDAQDummy.cxx TRESTDAQ/TRESTDAQManager.cxx)

target_include_directories(RESTDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/TRESTDAQ ${rest_include_dirs})

target_link_libraries(RESTDAQ ${lnklib} -lCore RestRaw)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION .
        COMPONENT install
        )

install(TARGETS RESTDAQ DESTINATION lib)



add_library(DCC SHARED DCC/endofevent_packet.c DCC/ethpacket.c DCC/gblink.c DCC/pedestal_packet.c)

target_include_directories(DCC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/DCC)

add_library(RESTDAQ SHARED TRESTDAQ/TRESTDAQ.cxx TRESTDAQ/TRESTDAQDCC.cxx TRESTDAQ/TRESTDAQDummy.cxx TRESTDAQ/TRESTDAQManager.cxx)

target_include_directories(RESTDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/TRESTDAQ ${CMAKE_CURRENT_SOURCE_DIR}/DCC ${rest_include_dirs})

target_link_libraries(RESTDAQ DCC ${lnklib} -lCore RestRaw)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION .
        COMPONENT install
        )

install(TARGETS DCC DESTINATION lib)

install(TARGETS RESTDAQ DESTINATION lib)


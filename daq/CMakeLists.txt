
add_library(DCC DCC/endofevent_packet.c DCC/ethpacket.c DCC/gblink.c DCC/pedestal_packet.c)

target_include_directories(DCC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/DCC )

add_library(TRESTDAQ TRESTDAQ/TRESTDAQ.cxx TRESTDAQ/TRESTDAQDCC.cxx)

target_include_directories(TRESTDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/TRESTDAQ ${CMAKE_CURRENT_SOURCE_DIR}/DCC ${rest_include_dirs} )

target_link_libraries(TRESTDAQ DCC -lCore RestRaw)



include_directories(${incdir})

add_library(RestDAQ SHARED TRESTDAQ.cxx TRESTDAQSocket.cxx DCCPacket.cxx TRESTDAQDCC.cxx FEMINOSPacket.cxx ARCPacket.cxx TRESTDAQFEMINOS.cxx TRESTDAQARC.cxx TRESTDAQDummy.cxx TRESTDAQManager.cxx)

target_include_directories(RestDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${rest_include_dirs})

target_link_libraries(RestDAQ ${lnklib} RestRaw)

install(TARGETS RestDAQ DESTINATION lib)


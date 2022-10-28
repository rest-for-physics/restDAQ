
add_library(RESTDAQ SHARED TRESTDAQ.cxx TRESTDAQSocket.cxx DCCPacket.cxx TRESTDAQDCC.cxx FEMINOSPacket.cxx TRESTDAQFEMINOS.cxx TRESTDAQDummy.cxx TRESTDAQManager.cxx)

target_include_directories(RESTDAQ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIRS})

target_link_libraries(RESTDAQ PUBLIC ${LINK_LIBRARIES} RestFramework RestRaw)

install(TARGETS RESTDAQ DESTINATION lib)

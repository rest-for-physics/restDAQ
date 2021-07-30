#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Setup the project

project(restDAQ)
message("===============  ${PROJECT_NAME}  ==============")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake ${CMAKE_MODULE_PATH})

# Find ROOT
find_package(ROOT REQUIRED COMPONENTS RIO Geom)
execute_process(COMMAND root-config --cflags OUTPUT_VARIABLE ROOT_CFLAGS)
string(STRIP ${ROOT_CFLAGS} ROOT_CFLAGS)
message("-- Found ROOT version: ${ROOT_VERSION} with compilation flags: ${ROOT_CFLAGS} and libraries: ${ROOT_LIBRARIES}")

# Find REST
find_package(REST REQUIRED)
message("-- Found REST version: ${REST_VERSION} with libraries: ${REST_LIBRARIES}")
# TODO: Implement a check for '-lRestRaw' in ${REST_LIBRARIES} (and exit if not found)
# Still if we use -DREST_DAQ=ON at the framework compilation it will enable RawLib by default

if (NOT CMAKE_CXX_FLAGS)
    SET(CMAKE_CXX_FLAGS "-std=c++1y") # We should use the c++ flags from ${ROOT_CFLAGS}
endif ()

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Set include and lib
set(incdir ${PROJECT_SOURCE_DIR}/include ${ROOT_INCLUDE_DIRS} ${REST_INCLUDE_DIRS})
set(lnklib ${ROOT_LIBRARIES} -lGui RestRaw)
string(STRIP "${lnklib}" lnklib)

add_subdirectory(daq)
add_subdirectory(gui)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Add execuatable and link to restDAQ libraries
add_executable(restDAQManager restDAQManager.cxx)
target_link_libraries(restDAQManager LINK_PUBLIC RESTDAQ ${lnklib})

#----------------------------------------------------------------------------
# Copy restDAQManager to the install directory
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION .
        COMPONENT install
        )

install(TARGETS restDAQManager DESTINATION bin)

#----------------------------------------------------------------------------
# Copy macros to the install directory
file(GLOB_RECURSE MAC "${CMAKE_CURRENT_SOURCE_DIR}/macros/*")
INSTALL(FILES ${MAC} DESTINATION ./macros/daq)

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
#install(TARGETS ${PROJECT_NAME} DESTINATION bin)
set(package_added "${PROJECT_NAME}")
set(package_added ${package_added} PARENT_SCOPE)


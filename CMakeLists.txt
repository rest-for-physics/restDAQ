#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Setup the project

project(restDAQ)
message("===============  ${PROJECT_NAME}  ==============")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake ${CMAKE_MODULE_PATH})

# Find ROOT
find_package(ROOT REQUIRED COMPONENTS Gui)
execute_process(COMMAND root-config --cflags OUTPUT_VARIABLE ROOT_CFLAGS)
string(STRIP ${ROOT_CFLAGS} ROOT_CFLAGS)
message("-- Found ROOT version: ${ROOT_VERSION} with compilation flags: ${ROOT_CFLAGS} and libraries: ${ROOT_LIBRARIES}")

#Find REST
IF(NOT DEFINED REST_PATH)
        IF(DEFINED ENV{REST_PATH})
                set(REST_PATH $ENV{REST_PATH})
        ELSE()
                message(FATAL_ERROR "ERROR ::: REST_PATH must be defined as an environment variable and point to REST install directory")
                return()
        ENDIF()
ENDIF()
set(CMAKE_INSTALL_PREFIX ${REST_PATH})
message(STATUS "Using REST PATH: " ${REST_PATH})

message("-- Found REST version: ${REST_VERSION} with libraries: ${REST_LIBRARIES}")
# TODO: Implement a check for '-lRestRaw' in ${REST_LIBRARIES} (and exit if not found)
# Still if we use -DREST_DAQ=ON at the framework compilation it will enable RawLib by default

if (NOT CMAKE_CXX_FLAGS)
    SET(CMAKE_CXX_FLAGS "-std=c++1y") # We should use the c++ flags from ${ROOT_CFLAGS}
endif ()

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Set include and lib
if(DEFINED rest_include_dirs)
	set(incdir ${PROJECT_SOURCE_DIR}/daq ${ROOT_INCLUDE_DIRS} ${rest_include_dirs})
else()
	set(incdir ${PROJECT_SOURCE_DIR}/daq ${ROOT_INCLUDE_DIRS} ${REST_PATH}/include)
endif()


set(lnklib ${ROOT_LIBRARIES} RestRaw)
string(STRIP "${lnklib}" lnklib)

add_subdirectory(daq)
add_subdirectory(gui)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Add execuatable and link to restDAQ libraries
add_executable(restDAQManager restDAQManager.cxx)
target_link_libraries(restDAQManager LINK_PUBLIC RestDAQ ${lnklib})

#----------------------------------------------------------------------------
# Copy restDAQManager to the install directory
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


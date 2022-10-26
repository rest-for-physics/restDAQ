#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Setup the project

project(restDAQ)

set(CMAKE_CXX_STANDARD 17)

if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    # this will only be accessed when build restG4 as a standalone package, not when building the framework
    message(STATUS "${PROJECT_NAME} standalone build")
    CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
    find_package(REST REQUIRED)
    string(FIND ${REST_LIBRARIES} "RestRaw" REST_RAW_FOUND)
    if (${REST_RAW_FOUND} EQUAL -1)
        message(SEND_ERROR "REST found but 'RestRaw' library is not present. Please install REST with RawLib")
    endif ()
endif ()

message("-- Found REST version: ${REST_VERSION} with libraries: ${REST_LIBRARIES}")

message("===============  ${PROJECT_NAME}  ==============")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake ${CMAKE_MODULE_PATH})

# Find ROOT
find_package(ROOT REQUIRED COMPONENTS Gui CONFIG)
execute_process(COMMAND root-config --cflags OUTPUT_VARIABLE ROOT_CFLAGS)
string(STRIP ${ROOT_CFLAGS} ROOT_CFLAGS)
message("-- Found ROOT version: ${ROOT_VERSION} with compilation flags: ${ROOT_CFLAGS} and libraries: ${ROOT_LIBRARIES}")

#Find REST
if (NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${REST_PATH})
endif ()

message(STATUS "Package ${PROJECT_NAME} will be installed in ${CMAKE_INSTALL_PREFIX}")

# TODO: Implement a check for '-lRestRaw' in ${REST_LIBRARIES} (and exit if not found)
# Still if we use -DREST_DAQ=ON at the framework compilation it will enable RawLib by default

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Set include and lib

set(INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include ${ROOT_INCLUDE_DIRS} ${REST_INCLUDE_DIRS})

set(LINK_LIBRARIES ${ROOT_LIBRARIES} RestRaw)
string(STRIP "${LINK_LIBRARIES}" LINK_LIBRARIES)

add_subdirectory(daq)
add_subdirectory(gui)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Add executable and link to restDAQ libraries

include(FetchContent)

# External dependencies
# cli11 (CLI parser)
FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.2.0
)
FetchContent_MakeAvailable(cli11)

set(EXECUTABLE_NAME restDAQ)

add_executable(${EXECUTABLE_NAME} main.cxx)

target_link_libraries(${EXECUTABLE_NAME} PUBLIC RESTDAQ RestDAQGUI ${LINK_LIBRARIES} ${ROOT_LIBRARIES} RestRaw PRIVATE CLI11::CLI11)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PUBLIC ${INCLUDE_DIRS})

message(STATUS "include directories: ${INCLUDE_DIRS}")

#----------------------------------------------------------------------------
# Copy restDAQManager to the install directory
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

#----------------------------------------------------------------------------
# Copy macros to the install directory
file(GLOB_RECURSE MAC "${CMAKE_CURRENT_SOURCE_DIR}/macros/*")
INSTALL(FILES ${MAC} DESTINATION ./macros/daq)

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
#install(TARGETS ${PROJECT_NAME} DESTINATION bin)

if (NOT ${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    set(package_added "${PROJECT_NAME}")
    set(package_added ${package_added} PARENT_SCOPE)
endif ()

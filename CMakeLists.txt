#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Setup the project
project(restDAQ)
message("===============  restDAQ  ==============")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake ${CMAKE_MODULE_PATH})

#find REST 
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


if(NOT CMAKE_CXX_FLAGS)
SET(CMAKE_CXX_FLAGS "-std=c++1y")
endif()
include(FindROOT)

#-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
#Set include and lib
if(DEFINED rest_include_dirs)
	set(incdir ${PROJECT_SOURCE_DIR}/include ${ROOT_INCLUDE_DIRS} ${rest_include_dirs} )
else()
	set(incdir ${PROJECT_SOURCE_DIR}/include ${ROOT_INCLUDE_DIRS} ${REST_PATH}/include )
endif()

# WARNING. The best would be to check here if the library RestRaw is available!!
# Still if we use -DREST_DAQ=ON at the framework compilation it will enable RawLib by default
set(lnklib ${ROOT_LIBRARIES} -lGui RestRaw )
string(STRIP "${lnklib}" lnklib)

add_subdirectory(daq)
add_subdirectory(programs)

#----------------------------------------------------------------------------
# Copy all scripts to the build directory, i.e. the directory in which we
# build sphereSimLab. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory.



#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
#install(TARGETS restDAQ DESTINATION bin)
set(package_added "restDAQ")
set(package_added ${package_added} PARENT_SCOPE)


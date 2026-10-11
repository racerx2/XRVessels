# XR Vessel add-ons for OpenOrbiter Space Flight Simulator - Linux port
# Copyright (C) 2026 racerx2; based on the XR vessels, Copyright (C) 2006-2025 Douglas Beachy
# Licensed under the GNU General Public License version 3 or any later version (see LICENSE)

# not upstream: Linux build of the XR vessels (XR1, XR2, XR5) for the native Linux Orbiter
cmake_minimum_required(VERSION 3.19)
project(XRVessels LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)
endif()

# same as Orbiter's own modules
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
add_compile_options(-Wformat)

set(ORBITER_ROOT "" CACHE PATH "Orbiter folder (Modules/, Orbitersdk/, Config/)")
set(ORBITER_CMAKE_DIR "${ORBITER_ROOT}/cmake" CACHE PATH "folder with Orbiter's rcresources.cmake and rc2cpp.py")
if(NOT EXISTS "${ORBITER_ROOT}/Orbitersdk/include/OrbiterAPI.h")
	message(FATAL_ERROR "ORBITER_ROOT must be the Orbiter folder (with Orbitersdk/include)")
endif()
if(NOT EXISTS "${ORBITER_CMAKE_DIR}/rcresources.cmake")
	message(FATAL_ERROR "ORBITER_CMAKE_DIR must hold rcresources.cmake and rc2cpp.py")
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${ORBITER_ROOT}" CACHE PATH "" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

set(SDK_DIR ${ORBITER_ROOT}/Orbitersdk)
set(ORBITER_SOURCE_SDK_INCLUDE_DIR ${SDK_DIR}/include) # rc2cpp include path
include(${ORBITER_CMAKE_DIR}/rcresources.cmake)

set(XR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/XRVessels)

# framework (framework.vcxproj)
add_library(xrframework STATIC
	${XR_DIR}/framework/framework/Area.cpp
	${XR_DIR}/framework/framework/AreaGroup.cpp
	${XR_DIR}/framework/framework/Component.cpp
	${XR_DIR}/framework/framework/ConfigFileParser.cpp
	${XR_DIR}/framework/framework/FileList.cpp
	${XR_DIR}/framework/framework/InstrumentPanel.cpp
	${XR_DIR}/framework/framework/RegKeyManager.cpp
	${XR_DIR}/framework/framework/Vessel3Ext.cpp
	${XR_DIR}/framework/framework/VesselConfigFileParser.cpp
	${XR_DIR}/framework/framework/XRGrappleTargetVessel.cpp
	${XR_DIR}/framework/framework/XRPayload.cpp
	${XR_DIR}/framework/framework/xrpayloadbay.cpp
	${XR_DIR}/framework/framework/XRPayloadBaySlot.cpp
	${XR_DIR}/framework/framework/XRGdi.cpp # not upstream: GDI counterparts on QPainter
)
target_include_directories(xrframework PUBLIC ${XR_DIR}/framework/framework ${SDK_DIR}/include ${SDK_DIR}/XRSound)
target_link_libraries(xrframework PUBLIC
	${SDK_DIR}/lib/libOrbitersdk.a
	${SDK_DIR}/lib/libDlgCtrl.a
	${SDK_DIR}/XRSound/libXRSound.a
	Qt6::Widgets
)

# XR1Lib (XR1Lib.vcxproj): compiled with the XR1's resource.h, as on Windows
file(GLOB XR1LIB_SOURCES ${XR_DIR}/DeltaGliderXR1/XR1Lib/*.cpp)
add_library(XR1Lib STATIC ${XR1LIB_SOURCES})
target_include_directories(XR1Lib PUBLIC ${XR_DIR}/DeltaGliderXR1/XR1Lib PRIVATE ${XR_DIR}/DeltaGliderXR1/DeltaGliderXR1)
target_link_libraries(XR1Lib PUBLIC xrframework)

# vessel modules: <name>.so in Modules/, resources from the module's .rc
function(xr_module name dir)
	file(GLOB sources ${dir}/*.cpp)
	add_library(${name} SHARED ${sources})
	orbiter_rc_resources(${name} ${dir}/${name}.rc)
	target_include_directories(${name} BEFORE PRIVATE ${dir})
	target_link_libraries(${name} PRIVATE XR1Lib)
	install(TARGETS ${name} LIBRARY DESTINATION Modules)
endfunction()

xr_module(DeltaGliderXR1 ${XR_DIR}/DeltaGliderXR1/DeltaGliderXR1)
xr_module(XR2Ravenstar ${XR_DIR}/XR2Ravenstar/XR2Ravenstar)
xr_module(XR5Vanguard ${XR_DIR}/XR5Vanguard/XR5Vanguard)

# data: the repo's Orbiter/ tree (without the unfinished XR3) and the prefs files; existing files are never overwritten
install(CODE "set(XR_SRC \"${CMAKE_CURRENT_SOURCE_DIR}\")")
install(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/InstallData.cmake)

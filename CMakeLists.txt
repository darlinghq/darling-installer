project(darling-installer)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb")

include(FindLibxml2)

find_package(Libxml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(darling-installer
	src/archive/FileReader.cpp
	src/archive/XARArchive.cpp
	src/archive/SubReader.cpp
	src/archive/InflateReader.cpp
	src/archive/main-test.cpp
)

target_link_libraries(darling-installer -lz ${LIBXML2_LIBRARY})
#install(TARGETS darling-installer RUNTIME DESTINATION bin)


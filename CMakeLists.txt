project(darling-installer)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb")

include(FindLibxml2)

find_package(Libxml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(installer
	src/archive/FileReader.cpp
	src/archive/XARArchive.cpp
	src/archive/SubReader.cpp
	src/archive/ArchivedFileReader.cpp
	src/archive/GzipReader.cpp
	src/archive/CPIOArchive.cpp
	src/archive/PbzxReader.cpp
	src/bom/BOMStore.cpp
        src/bom/BOMBTree.cpp
        src/bom/BOMFilesBTree.cpp
	src/main-test.cpp
)

target_link_libraries(installer -lz -lbz2 -llzma ${LIBXML2_LIBRARY})
install(TARGETS installer RUNTIME DESTINATION libexec/darling/usr/bin)


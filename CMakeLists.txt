project(darling-installer)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -nostdinc -nostdinc++ -std=c++11 -ggdb")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib -Wl,--version-script=${DARLING_TOP_DIRECTORY}/darwin.map")

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/darling")
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_definitions(-DRECEIPTS_DIR="/private/var/db/receipts")

include_directories(${DARLING_TOP_DIRECTORY}/src/external/libxml2/include)
include_directories(${CMAKE_BINARY_DIR}/src/external/libxml2/include)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/zlib)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/bzip2)
include_directories(${DARLING_TOP_DIRECTORY}/src/libc/fbsdcompat)
include_directories(${DARLING_TOP_DIRECTORY}/src/libc/include/FreeBSD)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/liblzma/src/liblzma/api)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/libcxx/include)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/icu/icuSources/common)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/corefoundation/Headers)
include_directories(${CMAKE_BINARY_DIR}/src/external/corefoundation/Headers)
include_directories(${DARLING_TOP_DIRECTORY}/src/external/foundation/Headers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

include(darling_exe)
include(InstallSymlink)

add_darling_executable(installer
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
        src/pkg/DistributionXml.cpp
        src/pkg/PackageInfoXml.cpp
        src/pkg/ReceiptsDb.cpp
        src/pkg/Installer.cpp
        src/pkg/Uninstaller.cpp
        src/main.cpp
        src/main-lsbom.cpp
        src/main-installer.cpp
        src/main-uninstaller.cpp
        src/main-pkgutil.cpp
)

target_link_libraries(installer z lzma bz2 xml2 cxx CFF)
install(TARGETS installer RUNTIME DESTINATION libexec/darling/usr/bin)

set(BINARY_PACKAGING_MODE ON)
InstallSymlink(installer libexec/darling/usr/bin/uninstaller)
InstallSymlink(installer libexec/darling/usr/bin/pkgutil)

install(FILES installer.8 DESTINATION libexec/darling/usr/share/man/man8)

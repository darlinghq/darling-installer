project(darling-installer)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -nostdinc -nostdinc++ -std=c++11")

add_definitions(-DRECEIPTS_DIR="/private/var/db/receipts")

include_directories(
	${CMAKE_SOURCE_DIR}/src/external/icu/icuSources/common
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

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

target_link_libraries(installer z lzma bz2 xml2 cxx CoreFoundation)
install(TARGETS installer RUNTIME DESTINATION libexec/darling/usr/bin)

set(BINARY_PACKAGING_MODE ON)
InstallSymlink(installer libexec/darling/usr/bin/uninstaller)
InstallSymlink(installer libexec/darling/usr/bin/pkgutil)
InstallSymlink(installer libexec/darling/usr/bin/lsbom)

install(FILES installer.8 DESTINATION libexec/darling/usr/share/man/man8)


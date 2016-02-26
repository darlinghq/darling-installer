#include "ReceiptsDb.h"
#include <unistd.h>
#include <memory>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFData.h>
#include <fcntl.h>
#include <stdexcept>
#include <ctime>
#include <sstream>
#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "archive/FileReader.h"

static std::string cfstring2stdstring(CFStringRef str);
static CFStringRef string2cfstring(const char* str);
static void mkdirs(const char* dir);
static std::string getReceiptsPath(const char* identifier, const char* ext);

bool ReceiptsDb::getInstalledPackageInfo(const char* identifier, ReceiptsDb::InstalledPackageInfo& info)
{
	std::string path;
	CFDataRef data;
	CFDateRef date;
	CFDictionaryRef plist;
	std::unique_ptr<Reader> fileReader;
	std::unique_ptr<UInt8[]> fileData;
	
	path = getReceiptsPath(identifier, ".plist");
	
	if (::access(path.c_str(), F_OK) != 0)
		return false;
	
	fileReader.reset(new FileReader(path));
	fileData.reset(new UInt8[fileReader->length()]);
	
	if (fileReader->read(fileData.get(), fileReader->length(), 0) != fileReader->length())
		throw std::runtime_error("Short read in getInstalledPackageInfo()");
	
	data = CFDataCreate(nullptr, fileData.get(), fileReader->length());
	fileData.reset(nullptr);
	
	plist = (CFDictionaryRef) CFPropertyListCreateWithData(nullptr, data,
			kCFPropertyListImmutable, nullptr, nullptr);
	CFRelease(data);
	
	if (plist == nullptr)
		throw std::runtime_error("Failed to parse plist in getInstalledPackageInfo()");
	
	info.identifier = cfstring2stdstring((CFStringRef) CFDictionaryGetValue(plist, CFSTR("PackageIdentifier")));
	info.version = cfstring2stdstring((CFStringRef) CFDictionaryGetValue(plist, CFSTR("PackageVersion")));
	info.prefixPath = cfstring2stdstring((CFStringRef) CFDictionaryGetValue(plist, CFSTR("InstallPrefixPath")));
	info.installProcessName = cfstring2stdstring((CFStringRef) CFDictionaryGetValue(plist, CFSTR("InstallProcessName")));
	info.packageFileName = cfstring2stdstring((CFStringRef) CFDictionaryGetValue(plist, CFSTR("PackageFileName")));
	
	date = (CFDateRef) CFDictionaryGetValue(plist, CFSTR("InstallDate"));
	info.installDate = CFDateGetAbsoluteTime(date);
	
	CFRelease(plist);
	return true;
}

bool ReceiptsDb::getInstalledPackageInfo(const char* identifier, std::string& plistXml)
{
	std::string path;
	std::unique_ptr<Reader> fileReader;
	std::unique_ptr<UInt8[]> fileData;
	CFDataRef data;
	CFDictionaryRef plist;
	
	path = getReceiptsPath(identifier, ".plist");
	
	if (::access(path.c_str(), F_OK) != 0)
		return false;
	
	fileReader.reset(new FileReader(path));
	fileData.reset(new UInt8[fileReader->length()]);
	
	if (fileReader->read(fileData.get(), fileReader->length(), 0) != fileReader->length())
		throw std::runtime_error("Short read in getInstalledPackageInfo()");
	
	data = CFDataCreate(nullptr, fileData.get(), fileReader->length());
	fileData.reset(nullptr);
	
	plist = (CFDictionaryRef) CFPropertyListCreateWithData(nullptr, data,
			kCFPropertyListImmutable, nullptr, nullptr);
	CFRelease(data);
	
	data = CFPropertyListCreateData(nullptr, plist,
			kCFPropertyListXMLFormat_v1_0, 0, nullptr);
	CFRelease(plist);
	
	plistXml.assign((const char*) CFDataGetBytePtr(data), CFDataGetLength(data));
	CFRelease(data);
	
	return true;
}

std::shared_ptr<BOMStore> ReceiptsDb::getInstalledPackageBOM(const char* identifier)
{
	std::shared_ptr<BOMStore> store;
	std::string path;
	
	path = getReceiptsPath(identifier, ".bom");
	
	if (::access(path.c_str(), F_OK) != 0)
		return store;
	
	store.reset(new BOMStore(std::make_shared<FileReader>(path)));
	
	return store;
}

static std::string cfstring2stdstring(CFStringRef str)
{
	const char* cstr;
	std::unique_ptr<char[]> buffer;
	CFIndex maxLen;
	
	if (str == nullptr)
		return std::string();
	
	cstr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
	if (cstr != nullptr)
		return cstr;
	
	maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
	buffer.reset(new char[maxLen]);
	
	if (CFStringGetCString(str, buffer.get(), maxLen, kCFStringEncodingUTF8))
		return std::string(buffer.get(), CFStringGetLength(str));
	else
		return std::string();
}

static void mkdirs(const char* dir)
{
	std::string str;
	const char* p = dir;
	
	if (::mkdir(dir, 0755) == 0 || errno == EEXIST)
		return;
	
	while (p != nullptr)
	{
		p = strchr(p, '/');
		if (p == nullptr)
			str = dir;
		else
			str.assign(dir, p-dir);
		
		if (::mkdir(str.c_str(), 0755) != 0)
		{
			if (errno != EEXIST)
				break;
		}
		
		if (!p) break;
		p++;
	}
}

void ReceiptsDb::putInstalledPackageInfo(const char* identifier, const ReceiptsDb::InstalledPackageInfo& info)
{
	CFDictionaryRef plist;
	CFTypeRef keys[6], values[6];
	CFDataRef plistData;
	std::string path;
	int fd, wr;
	
	mkdirs(RECEIPTS_DIR);
	
	keys[0] = CFSTR("PackageIdentifier");
	keys[1] = CFSTR("PackageVersion");
	keys[2] = CFSTR("InstallPrefixPath");
	keys[3] = CFSTR("InstallProcessName");
	keys[4] = CFSTR("PackageFileName");
	keys[5] = CFSTR("InstallDate");
	
	values[0] = string2cfstring(info.identifier.c_str());
	values[1] = string2cfstring(info.version.c_str());
	values[2] = string2cfstring(info.prefixPath.c_str());
	values[3] = string2cfstring(info.installProcessName.c_str());
	values[4] = string2cfstring(info.packageFileName.c_str());
	values[5] = CFDateCreate(nullptr, info.installDate);
	
	plist = CFDictionaryCreate(nullptr, keys, values, 6,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	for (CFTypeRef v : values)
		CFRelease(v);
	
	plistData = CFPropertyListCreateData(nullptr, plist,
			kCFPropertyListBinaryFormat_v1_0, 0, nullptr);
	CFRelease(plist);
	
	if (plistData == nullptr)
		throw std::runtime_error("Cannot generate .plist");
	
	path = getReceiptsPath(identifier, ".plist");
	fd = ::open(path.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
	
	if (fd == -1)
	{
		std::stringstream ss;
		ss << "Cannot write plist file: " << strerror(errno) << std::endl;
		throw std::runtime_error(ss.str());
	}
	
	wr = ::write(fd, CFDataGetBytePtr(plistData), CFDataGetLength(plistData));
	CFRelease(plistData);
	
	if (wr == -1)
	{
		std::stringstream ss;
		ss << "Cannot write plist file: " << strerror(errno) << std::endl;
		
		::close(fd);
		throw std::runtime_error(ss.str());
	}
	
	::close(fd);
}

static CFStringRef string2cfstring(const char* str)
{
	return CFStringCreateWithCString(nullptr, str, kCFStringEncodingUTF8);
}

std::string getReceiptsPath(const char* identifier, const char* ext)
{
	std::string path;
	
	path = RECEIPTS_DIR;
	path += '/';
	path += identifier;
	path += ext;
	
	return path;
}

std::string ReceiptsDb::getInstalledPackageBOMPath(const char* identifier)
{
	return getReceiptsPath(identifier, ".bom");
}

std::set<std::string> ReceiptsDb::getInstalledPackages()
{
	std::set<std::string> rv;
	DIR* d;
	struct dirent* ent;
	
	d = opendir(RECEIPTS_DIR);
	if (!d)
		return rv;
	
	while ((ent = readdir(d)) != nullptr)
	{
		size_t len = strlen(ent->d_name);
		
		if (len > 6 && strcmp(ent->d_name + len - 6, ".plist") == 0)
		{
			ent->d_name[len - 6] = '\0';
			rv.insert(ent->d_name);
		}
	}
	
	closedir(d);
	return rv;
}

void ReceiptsDb::removePackage(const char* identifier)
{
	std::string path;
	
	path = getReceiptsPath(identifier, ".bom");
	::unlink(path.c_str());
	
	path = getReceiptsPath(identifier, ".plist");
	::unlink(path.c_str());
}


#include "installer.h"
#include <stdexcept>
#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <sstream>
#include <errno.h>
#include <unistd.h>
#include "archive/XARArchive.h"
#include "archive/FileReader.h"
#include "archive/GzipReader.h"
#include "archive/PbzxReader.h"
#include "archive/CPIOArchive.h"
#include "DistributionXml.h"
#include "PackageInfoXml.h"
#include "ReceiptsDb.h"
#include <CoreFoundation/CFDate.h>

int installPackage(const char* pkg, const char* target)
{
	std::shared_ptr<FileReader> file;
	std::shared_ptr<XARArchive> xar;
	std::shared_ptr<Reader> distributionFile;
	struct stat st;
	
	// DomainKey
	if (strcmp(target, "LocalSystem") == 0)
		target = "/";
	else if (target[0] != '/')
		throw std::runtime_error("Invalid target path");
	
	if (::stat(target, &st) != 0)
		throw std::runtime_error("Invalid target path");
	
	if (!S_ISDIR(st.st_mode))
		throw std::runtime_error("Invalid target path; not a directory");
	
	file.reset(new FileReader(pkg));
	xar.reset(new XARArchive(file));
	
	// If "Distribution" file exists, then read it and find out choices enabled by default (normally located in subdirectories)
	// If no "Distribution" file exists, then there are no subdirectories and we proceed with installation
	if (Reader* distr = xar->openFile("Distribution"))
	{
		std::unique_ptr<DistributionXml> distribution;
		std::vector<DistributionXml::Choice> choices;
		
		distributionFile.reset(distr);
		distribution.reset(new DistributionXml(distributionFile));
		
		std::cout << "installer: Package name is " << distribution->title() << std::endl;
		
		choices = distribution->choices();
		for (const DistributionXml::Choice& c : choices)
		{
			if (!c.selected)
				continue;
			
			std::cout << "installer: Installing selected choice " << c.title << std::endl;
			
			for (const std::string& pkgId : c.pkgref)
			{
				DistributionXml::PkgRef pkgref;
				
				if (!distribution->package(pkgId, pkgref))
				{
					std::stringstream ss;
					ss << "Cannot find <pkg-ref> with id '" << pkgId << "'";
					throw std::runtime_error(ss.str());
				}
				
				// std::cout << "installer: Installing package " << pkgId << " (" << pkgref.installKbytes << " KB)\n";
				installPayload(xar, pkgref.path.c_str() + 1, target);
			}
		}
	}
	else
	{
		return installPayload(xar, "", target);
	}
	
	return 0;
}

static std::shared_ptr<PackageInfoXml>
loadPackageInfo(std::shared_ptr<XARArchive> xar, const char* subdir)
{
	std::string path;
	std::shared_ptr<Reader> pkgInfoFile;
	
	path = subdir;
	if (!path.empty())
		path += '/';
	path += "PackageInfo";
	
	pkgInfoFile.reset(xar->openFile(path));
	if (!pkgInfoFile)
		throw std::runtime_error(path + " not found in .pkg");
	
	return std::make_shared<PackageInfoXml>(pkgInfoFile);
}

static void
extractPayload(std::shared_ptr<XARArchive> xar,
		std::shared_ptr<PackageInfoXml> pkgInfo,
		const char* subdir, const char* target)
{
	std::string path, name, installLocation;
	std::shared_ptr<Reader> cpioFile;
	std::unique_ptr<CPIOArchive> cpio;
	struct stat st;
	size_t basePathLen;
	
	path = subdir;
	if (!path.empty())
		path += '/';
	path += "Payload";
	
	cpioFile.reset(xar->openFile(path));
	
	if (GzipReader::isGzip(cpioFile))
		cpioFile.reset(new GzipReader(cpioFile));
	else if (PbzxReader::isPbzx(cpioFile))
		cpioFile.reset(new PbzxReader(cpioFile));
	
	cpio.reset(new CPIOArchive(cpioFile));
	
	path = target;
	if (path[path.length()-1] != '/') path += '/';
	
	installLocation = pkgInfo->installLocation();
	if (installLocation != "/")
		path += installLocation;
	if (path[path.length()-1] != '/') path += '/';
	
	basePathLen = path.length();
	
	while (cpio->next(name, st, cpioFile))
	{
		// The final path is a concatenation of target, pkgInfo->installLocation()
		// and name.
		path.resize(basePathLen);
		path += name;
		
		// std::cout << path << std::endl;
		
		if (S_ISDIR(st.st_mode))
		{
			if (::mkdir(path.c_str(), st.st_mode & 0777) != 0)
			{
				if (errno != EEXIST)
				{
					std::stringstream ss;
					ss << "Cannot mkdir " << path << ": " << strerror(errno);
					throw std::runtime_error(ss.str());
				}
			}
		}
		else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		{
			std::cout << "installer: Warning: not installing a device node "
					<< path << std::endl;
		}
		else if (S_ISLNK(st.st_mode))
		{
			char target[256];
			int rd;
			
			rd = cpioFile->read(target, sizeof(target)-1, 0);
			target[rd] = '\0';
			
			::unlink(path.c_str());
			if (::symlink(target, path.c_str()) != 0)
			{
				std::stringstream ss;
				ss << "Cannot create symlink " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
		}
		else
		{
			std::unique_ptr<char[]> buf(new char[4096]);
			uint64_t length, done = 0;
			int fd;
			
			::unlink(path.c_str());
			
			fd = ::open(path.c_str(), O_CREAT|O_WRONLY, st.st_mode);
			if (fd == -1)
			{
				std::stringstream ss;
				ss << "Cannot create file " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
			
			length = cpioFile->length();
			while (done < length)
			{
				int rd, wr;
				int32_t toread;
				
				toread = std::min<int32_t>(4096, length-done);
				rd = cpioFile->read(buf.get(), toread, done);
				
				if (rd != toread)
					throw std::runtime_error("Short read in extractPayload");
				
				wr = ::write(fd, buf.get(), rd);
				if (wr != rd)
				{
					std::stringstream ss;
					ss << "Cannot write file " << path << ": " << strerror(errno);
					throw std::runtime_error(ss.str());
				}
				
				done += rd;
			}
			
			::close(fd);
			::chmod(path.c_str(), st.st_mode);
		}
	}
}

static std::string concatPaths(std::string p1, std::string p2)
{
	std::string joined = p1 + "/" + p2;
	size_t pos = 0;
	
	while ((pos = joined.find("//", pos)) != std::string::npos)
	{
		joined.replace(pos, 2, "/");
	}
	
	return joined;
}

int installPayload(std::shared_ptr<XARArchive> xar, const char* subdir, const char* target)
{
	std::shared_ptr<PackageInfoXml> packageInfo;
	std::string identifier;
	
	ReceiptsDb::InstalledPackageInfo installedPackageInfo;
	
	packageInfo = loadPackageInfo(xar, subdir);
	identifier = packageInfo->identifier();
	
	std::cout << "installer: Installing package " << identifier
			<< " version " << packageInfo->version()
			<< " (" << packageInfo->installKBytes() << " KB)\n";
	
	if (ReceiptsDb::getInstalledPackageInfo(packageInfo->identifier().c_str(), installedPackageInfo))
	{
		std::cout << "installed: Uninstalling previous version "
				<< installedPackageInfo.version << std::endl;
		
		// TODO: uninstall
	}

	extractPayload(xar, packageInfo, subdir, target);
	
	// TODO: Copy BOM file
	// TODO: Write a ReceiptsDb::InstalledPackageInfo
	installedPackageInfo.identifier = identifier;
	installedPackageInfo.installDate = CFAbsoluteTimeGetCurrent();
	installedPackageInfo.installProcessName = "installer";
	installedPackageInfo.packageFileName = subdir; // TODO: subdir is empty for "RAW" packages
	installedPackageInfo.prefixPath = concatPaths(target, packageInfo->installLocation());
	installedPackageInfo.version = packageInfo->version();
	
	ReceiptsDb::putInstalledPackageInfo(identifier.c_str(), installedPackageInfo);
	
	return 0;
}

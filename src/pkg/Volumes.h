#ifndef VOLUMES_H
#define VOLUMES_H
#include <string>
#include <vector>

struct Volume
{
	std::string name, mountPoint, deviceNode, fsKind;
	unsigned long long size;      // bytes
	unsigned long long available; // bytes
};

// The root volume and the writable volumes mounted under /Volumes
std::vector<Volume> mountedVolumes();

#endif /* VOLUMES_H */

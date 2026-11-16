#include "Volumes.h"
#include <sys/mount.h>

std::vector<Volume> mountedVolumes()
{
	std::vector<Volume> rv;
	struct statfs* fs;
	int count = getmntinfo(&fs, MNT_NOWAIT);

	for (int i = 0; i < count; i++)
	{
		std::string mp = fs[i].f_mntonname;
		bool isRoot = (mp == "/");

		if (!isRoot && (mp.compare(0, 9, "/Volumes/") != 0 || (fs[i].f_flags & MNT_RDONLY)))
			continue;

		Volume v;
		v.mountPoint = mp;
		v.name = isRoot ? "System" : mp.substr(mp.rfind('/') + 1);
		v.deviceNode = fs[i].f_mntfromname;
		if (v.deviceNode.compare(0, 5, "/dev/") == 0)
			v.deviceNode.erase(0, 5);
		v.fsKind = fs[i].f_fstypename;
		v.size = (unsigned long long) fs[i].f_blocks * fs[i].f_bsize;
		v.available = (unsigned long long) fs[i].f_bavail * fs[i].f_bsize;
		rv.push_back(v);
	}
	return rv;
}

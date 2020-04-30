#include "archive/XARArchive.h"
#include "archive/FileReader.h"
#include <iostream>
#include "bom/BOMStore.h"
#include "bom/BOMFilesBTree.h"

/*
int main(int argc, char** argv)
{
	FileReader* file = new FileReader(argv[1]);
	XARArchive* xar = new XARArchive(file);


	delete xar;
	return 0;
}
*/

static void usage();

int main_lsbom(int argc, char** argv)
{
	if (argc != 2)
	{
		usage();
		return 1;
	}

	try
	{
		std::shared_ptr<FileReader> file (new FileReader(argv[1]));
		std::shared_ptr<BOMStore> store (new BOMStore(file));
		std::shared_ptr<BOMFilesBTree> tree;
		std::map<uint32_t, BOMPathElement> contents;
		
		tree = store->getFilesTree();
		tree->listDirectory(BOMFilesBTree::kRootDirectory, contents);
		
		for (std::pair<uint32_t, BOMPathElement> item : contents)
		{
			std::cout << item.second.name() << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	
    return 0;
}

static void usage()
{
	std::cerr << "lsbom <bom-file>\n";
}

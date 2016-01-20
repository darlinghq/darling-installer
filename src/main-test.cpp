#include "archive/XARArchive.h"
#include "archive/FileReader.h"
#include <iostream>
#include "bom/BOMStore.h"

/*
int main(int argc, char** argv)
{
	FileReader* file = new FileReader(argv[1]);
	XARArchive* xar = new XARArchive(file);


	delete xar;
	return 0;
}
*/

int main(int argc, char** argv)
{
    FileReader* file = new FileReader(argv[1]);
    BOMStore* store = new BOMStore(file);
    return 0;
}

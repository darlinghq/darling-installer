#include "XARArchive.h"
#include "FileReader.h"
#include <iostream>

int main(int argc, char** argv)
{
	FileReader* file = new FileReader(argv[1]);
	XARArchive* xar = new XARArchive(file);


	delete file;
	return 0;
}


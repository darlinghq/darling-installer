#include <cstring>

static const char* progname(const char* argv0);

int main_lsbom(int argc, char** argv);
int main_installer(int argc, char** argv);
int main_uninstaller(int argc, char** argv);

int main(int argc, char** argv)
{
	const char* pname;
	
	pname = progname(argv[0]);
	
	if (strcmp(pname, "lsbom") == 0)
		return main_lsbom(argc, argv);
	else if (strcmp(pname, "uninstaller") == 0)
		return main_uninstaller(argc, argv);
	else
		return main_installer(argc, argv);
}

const char* progname(const char* argv0)
{
	char* p;
	
	p = strrchr((char*) argv0, '/');
	if (p != nullptr)
		return p+1;
	else
		return argv0;
}

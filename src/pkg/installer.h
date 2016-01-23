#ifndef INSTALLER_H
#define INSTALLER_H
#include <memory>
#include "archive/XARArchive.h"

int installPackage(const char* pkg, const char* target);
int installPayload(std::shared_ptr<XARArchive> xar, const char* subdir, const char* target);

#endif /* INSTALLER_H */


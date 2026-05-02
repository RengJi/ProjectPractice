#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <io.h>

struct countinfo
{
	int linecnt;
	int filecnt;
};

struct countinfo CodeLineCount(const char* file);

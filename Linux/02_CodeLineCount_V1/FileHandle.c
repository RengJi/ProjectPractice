#include "FileHandle.h"

/*
* 功能：获得行数
* 参数：filename，需要统计文件路径
* 返回值：包含的代码行数,失败返回-1
*/
static int LineCount(const char* filename)
{
	int ret = 1;//行数计数器
	int c = 0;//字符接收器
	FILE* stream = NULL;//创建的流
	/*打开文件*/
	if ((stream = fopen(filename, "r+")) == NULL)
	{
		printf("打开文件失败了\n");
		return -1;
	}
	/*读取文件*/
	while ((c = fgetc(stream)) != EOF)
	{
		if (c == '\n')
			ret++;
	}
	/*关闭文件*/
	if (fclose(stream) == EOF)
	{
		printf("文件关闭失败\n");
		return -1;
	}
	return ret;
}



/*
* 功能：查找整个文件夹文本文件代码的行数
* 参数：file-需要统计的文件夹
* 返回值：行数信息
*/
struct countinfo CodeLineCount(const char* file)
{
	struct _finddata_t fileinfo;
	intptr_t filehandle;
	char space[1024];
	char filename[1024];
	int linecount = 0;
	struct countinfo ci = { 0,0 };
	sprintf(space, "%s\\*", file);
	if ((filehandle = _findfirst(space, &fileinfo)) == -1)
	{
		printf("寻找第一个文件失败\n");
		return;
	}
	do
	{
		if (fileinfo.name[0] == '.')//过滤掉目录文件
			continue;
		/*统计子文件夹*/
		if (fileinfo.attrib & _A_SUBDIR)
		{
			struct countinfo subci;
			sprintf(space, "%s\\%s", file, fileinfo.name);
			subci = CodeLineCount(space);
			ci.linecnt += subci.linecnt;
			ci.filecnt += subci.filecnt;
			continue;//跳过文件夹文件的后续处理
		}
		sprintf(filename, "%s\\%s", file, fileinfo.name);
		linecount = LineCount(filename);//这里需要传入完整路径
		ci.linecnt += linecount;
		ci.filecnt++;
		printf("文件%s 包含代码：%d行\n", filename, linecount);

	} while (_findnext(filehandle, &fileinfo) == 0);
	
	_findclose(filehandle);
	return ci;
}


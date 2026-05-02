#include "FileHandle.h"
#include <stdlib.h>

/*构建一个文件代码的统计器
*测试1："H:\\KeYanChengGuo\\ProgramByC\\CodeLineCount_V1\\lwip-2.1.3"
* 测试2："H:\\KeYanChengGuo\\ProgramByC\\CodeLineCount_V1\\test"
*/
int main()
{
	struct countinfo ci;
	char file[] = "H:\\KeYanChengGuo\\ProgramByC\\CodeLineCount_V1\\lwip-2.1.3";
	ci = CodeLineCount(file);
	printf("---------------------------------------------------------------------------\n");
	printf("总共统计了 %d 个文件，包含了 %d 行代码\n",ci.filecnt,ci.linecnt);
	system("pause");
	return 0;
}
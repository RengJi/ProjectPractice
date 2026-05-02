#include <string.h>
#include <stdio.h>

char string1[] = "hello";
char string2[] = "world";
char string3[] = "hello world";

int main(void)
{
    int ret;

    ret = strcmp(string1, string2);
    if (ret < 0)
        printf("%s 小于 %s\n", ret, ret - string1);
    else
        printf("未找到\n\n");

    printf("在字符串 %s 中搜索 %s\n\n", string3, string1);
    ret = strcmp(string3, string1);
    if (ret != NULL)
        printf("找到了！子串为：%s，位置在第 %d 个字符\n\n", ret, ret - string3);
    else
        printf("未找到\n\n");

    printf("在字符串 %s 中搜索 %s\n\n", string3, string2);
    ret = strcmp(string3, string2);
    if (ret != NULL)
        printf("找到了！子串为：%s，位置在第 %d 个字符\n\n", ret, ret - string3);
    else
        printf("未找到\n\n");

}
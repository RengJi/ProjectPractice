#include <stdio.h>
#include <stdbool.h>
#include <string.h>

FILE *stream1 = NULL;

bool is_empty_str(char *str) {
    int i = 0, len = strlen(str);
    for (i = 0; i<len; i++) {
        if (str[i] != '\n'&&str[i] != ' '&&str[i] != '\t') {
            return false;
        }
    }
    return true;
}

int main(void)
{
    char line[4096] = { 0 };

    time_t timestamp = 0;
    time_t elapsed_sec = 0;
    unsigned int line_len = 0;
    char *cmd_P = NULL;
    char cmd_str[1024] = { 0 };
    int ret;

    //打开文本文件用于读操作，文件必须存在
    if ((stream1 = fopen("D:\\test\\data1.txt", "r")) == NULL)
    {
        printf("D:\\test\\data1.txt 打开失败\n");
        return -1;
    }
    else
    {
        printf("D:\\test\\data1.txt 打开成功，内容如下：\n");
    }
    //从文件按字符读取数据，并将数据打印在屏幕上，直到文件结束
    while (fgets(line, sizeof(line), stream1) != NULL)
    {
        line_len = strlen(line);
        printf("[%d]%s", line_len, line);
        //如果是空行则跳过
        if (line_len == 0) {
            continue;
        }

       line[line_len - 1] = '\0';
        
        //printf("%s", line);

        memset(cmd_str, 0, sizeof(cmd_str));
        ret = 0;
        //如果是带时间戳的记录格式
        ret = sscanf(line, ": %ld:%ld;%[\n]", &timestamp, &elapsed_sec, cmd_str);

        if (3 == ret) {
            if (is_empty_str(cmd_str)) {
                printf("ret ===3\n");
                continue;
            }
            cmd_P = &cmd_str[0];
        }
        //否则不是带时间戳的记录格式，则将整个行作为命令提交
        else {
            if (is_empty_str(line)) {
                printf("ret ==%d\n", ret);
                continue;
            }
            timestamp = 3333333;
            cmd_P = &line[0];
        }
        printf("parse ret:[%d], time:[%d]", ret, timestamp);
        printf("cmd:[%s]\n\n", cmd_P);
    }
    printf("\n");
    // 如果stream1为非空
    if (stream1)
    {
        if (fclose(stream1) == EOF)
        {
            printf("D:\\test\\data1.txt 关闭失败\n");
        }
    }
    getchar();
}
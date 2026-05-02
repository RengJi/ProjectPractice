#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

/* 数据内容 */
typedef struct {
        char name[32];    /**< 系统名字 */
        char version[64]; /**< 系统版本 */
        char arch[32];    /**< 系统架构 */
        float uptime;     /**< 开机时间 */
        float cpu_usage;  /**< CPU 使用率 */
        float io_usage;   /**< IO 使用率 */
        float mem_usage;  /**< 内存使用率 */
        float disk_usage; /**< 硬盘使用率 */
        long timestamp;   /**< 记录时刻的时间戳 */
} client_info_t;

/**
 * @brief 将时间戳转换成可读格式
 * 
 * @param timestamp 时间戳
 * @return char* 转换完的字符串
 */
char *timestamp_to_timestring(time_t timestamp)
{
        static char time[24] = {0};

        /* 转换时间戳 */
        struct tm *p_time;
        p_time = localtime(&timestamp);

        /* 生成可读格式 */
        sprintf(time, "%04d-%02d-%02d %02d:%02d:%02d\n", p_time->tm_year + 1900, p_time->tm_mon + 1,
                p_time->tm_mday, p_time->tm_hour, p_time->tm_min, p_time->tm_sec);

        return time;
}

/**
 * @brief 将开机时间从秒转换成可读的时间格式字符串
 * 
 * @param uptime 开机时间
 * @param buf 接收转换完字符串的缓冲区
 * @param buf_size 缓冲区大小
 */
void format_uptime(double uptime, char *buf, int buf_size)
{
        long total_seconds = (long)uptime;
        int seconds = 0, minutes = 0, hours = 0, days = 0;

        /* 计算时间 */
        days = total_seconds / (3600 * 24);
        total_seconds = total_seconds % (3600 * 24);
        hours = total_seconds / 3600;
        total_seconds = total_seconds % 3600;
        minutes = total_seconds / 60;
        seconds = total_seconds % 60;

        /* 统计时间 */
        snprintf(buf, buf_size - 1, "系统已经开机: %d 天 %d 小时 %d 分钟 %d 秒\n", days, hours, minutes, seconds);
}

/**
 * @brief 获取最新数据
 * 
 * @param file_name 共享数据文件
 * @param last_line_buf 接收缓冲区
 * @param last_line_buf_size 接收缓冲区大小
 */
void get_file_last_line(const char *file_name, char *last_line_buf, int last_line_buf_size)
{
        FILE *fs = fopen(file_name, "r");
        if (fs == NULL) {
                printf("Failed to open the %s\n", file_name);
                return;
        }    

        /* 循环读取 */
        char line[256] = {0};
        while(fgets(line,sizeof(line) - 1, fs) != NULL && strlen(line) > 0) {
                strncpy(last_line_buf, line, last_line_buf_size - 1);
                memset(line, 0, sizeof(line));
        }

        fclose(fs);

        /* 打印一下看看最后一行内容 */
        printf("最后一行数据为: %s", last_line_buf);
}

/**
 * @brief 提取获取的内容信息
 * 
 * @param info 接收提取到的内容
 * @param data 读取到的数据
 */
void decode_data_content(client_info_t *info, char *data)
{
        /*  提取数据 */
        int ret = sscanf(data, "UPDATE Name,%31[^,],Version,%63[^,],Arch,%31[^,],Uptime,%f,CPU,%f,IO,%f,Memory,%f,Disk,%f,time,%lu",
                                info->name, info->version, info->arch, &info->uptime, &info->cpu_usage, &info->io_usage, &info->mem_usage, &info->disk_usage, &info->timestamp);
        if (ret < 9) {
                printf("提取数据不完整\n");
        }
}


/**
 * @brief 发送网页
 * 
 * @param client_sockfd 已连接套接字
 */
void send_html(int client_sockfd)
{
        /* 响应头和响应行 */
        char *header_line = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n\r\n";
        
        /* 获取并处理要发送的数据 */
        char body[4096] = {0};
        char last_line_data[1024] = {0};
        char uptime_str[128];
        char *updatetime_str;
        client_info_t info_buf;

        time_t ts = time(NULL);
        get_file_last_line("data.txt", last_line_data, sizeof(last_line_data));
        decode_data_content(&info_buf, last_line_data);
        format_uptime(info_buf.uptime, uptime_str, sizeof(uptime_str));
        updatetime_str = timestamp_to_timestring(ts);

        /* 响应体 */
        snprintf(body, sizeof(body) - 1, 
        "<html>\r\n"
        "<head><title>浮生云</title></head>\r\n"
        "<body>\r\n"
            "<p>系统名称: %s</p>\r\n"
            "<p>系统版本: %s</p>\r\n"
            "<p>系统架构: %s</p>\r\n"
            "<p>开机时长: %s</p>\r\n"
            "<p>CPU使用率: %.2f%%</p>\r\n"
            "<p>IO使用率: %.2f%%</p>\r\n"
            "<p>内存使用率: %.2f%%</p>\r\n"
            "<p>磁盘使用率: %.2f%%</p>\r\n"
            "<p>数据更新时间: %s</p>\r\n"
        "</body>\r\n"
        "</html>\r\n",
        info_buf.name, info_buf.version, info_buf.arch, uptime_str, info_buf.cpu_usage, info_buf.io_usage,
        info_buf.mem_usage, info_buf.disk_usage, updatetime_str);

        /* 发送响应头和响应体 */
        send(client_sockfd, header_line, strlen(header_line), 0);
        send(client_sockfd, body, strlen(body), 0);

}

/**
 * @brief 处理客户端连接
 * 
 * @param client_sockfd 连接的客户端
 */
void client_handle(int client_sockfd)
{
        char recv_buf[4096] = {0};
        ssize_t ret = recv(client_sockfd, recv_buf, sizeof(recv_buf), 0);
        if (ret < 0) {
                printf("网页接收请求失败\n");
                return;
        } else if (ret == 0) {
                printf("TCP 服务关闭");
                return;
        } else {
                /* 无论接收到什么这里都发送这个网页内容 */
                send_html(client_sockfd);
        }
}

int main(void)
{
        int client_sockfd, sever_sockfd;
        int ret = 0;
        int opt = 1;
        int port = 8080;
        struct sockaddr_in sever_addr, client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        

        /* 创建一个 IPV4 TCP 和套接字 */
        sever_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sever_sockfd == -1) {
                perror("socket");
                return errno;
        }
        printf("Succeed to apply sever_socket\n");

        /* 设置端口复用 */
        ret = setsockopt(sever_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret == -1) {
                perror("setsockopt");
                return errno;
        }

        printf("Succeed to set sockopt\n");

        /* 设置服务端地址 */
        sever_addr.sin_family = AF_INET;
        sever_addr.sin_addr.s_addr = inet_addr("192.168.1.121");
        sever_addr.sin_port = htons(port);

        /* 绑定套接字 */
        ret = bind(sever_sockfd, (const struct sockaddr *)&sever_addr, sizeof(sever_addr));
        if (ret == -1) {
                perror("bind");
                return errno;
        }
        printf("Succeed to bind socket\n");

        /* 监听套接字 */
        ret = listen(sever_sockfd, 2);
        if (ret == -1) {
                perror("listen");
                return errno;
        }
        printf("Succeed to listen socket\n");

        /* 轮询等待客户端连接 */
        while (1) {
                client_sockfd = accept(sever_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client_sockfd == -1) {
                        perror("accept");
                        return errno;
                }
                printf("client linked, ip[%s], port[%d]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                client_handle(client_sockfd);
                close(client_sockfd);
        }

        close(sever_sockfd);
        return 0;
}

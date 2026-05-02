#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <FSTP.h>

#define BUF_SIZE 1024

/**
 * @brief 添加接收到的信息到共享文件内
 * 
 * @param filename 存储信息的文件
 * @param content  接收到的客户端发送来的信息
 * @param t        时间戳
 */
void add_data_to_file(const char *filename, const char *content, time_t t);

/**
 * @brief 发送时间戳到客户端
 * 
 * @param client_sockfd 已连接套接字
 * @param t             需要发送的时间戳
 */
void send_time_to_client(int client_sockfd, time_t t);

/**
 * @brief 处理客户端连接
 * 
 * @param client_sockfd 已连接套接字
 */
void handle_client(int client_sockfd);

int main(void) 
{
        int client_sockfd, sever_sockfd;
        int ret = 0;
        int opt = 1;
        int port = 8081;
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

                handle_client(client_sockfd);
                close(client_sockfd);
        }

        close(sever_sockfd);
        return 0;
}

void add_data_to_file(const char *filename, const char *content, time_t t)
{
        char buf[BUF_SIZE] = {0};

        FILE *fd = fopen(filename, "a"); //追加模式打开文件

        /* 处理数据 */
        strcpy(buf, content);
        sprintf(buf + strlen(buf) - 2, ",time,%ld\r\n", (long) t); //覆盖掉原来的 "\r\n" 并加上时间

        fprintf(fd, "%s", buf); //写入完整数据
        printf("Succeed to save data: %s", buf);

        fclose(fd);
}

void send_time_to_client(int client_sockfd, time_t t)
{
        char send_buf[BUF_SIZE] = {0};
        sprintf(send_buf, "TIME %ld\r\n", (long)t);
        
        /* 发送数据给客户端 */
        ssize_t ret = send(client_sockfd, send_buf, strlen(send_buf), 0);
        if (ret < 0) {
                printf("Failed to send time\n");
        } else if (ret == 0) {
                printf("tcp close\n");
        } else {
                printf("Succeed to send data: %s", send_buf);
        }
}

void handle_client(int client_sockfd)
{
        char recv_buf[BUF_SIZE] = {0};
        time_t timestamp = 0;

        while (1) {
                ssize_t ret = recv(client_sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
                if (ret < 0) {
                  printf("Failed to receive the data\n");
                  return;
                } else if (ret == 0) {
                    printf("tcp close\n");
                    return;
                } else {
                        recv_buf[ret] = '\0';
                        time(&timestamp);
                        send_time_to_client(client_sockfd, timestamp);

                        if (verify_fstp_update_cmd(recv_buf)) {
                                add_data_to_file("data.txt", recv_buf, timestamp);
                        } else {
                                printf("The data recieved is error\n");
                                return;
                        }
                       
                } 
        }

        
}

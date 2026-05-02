#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <FSTP.h>

/* cpu 时间 */
typedef struct {
    __uint64_t user_time;       //用户态运行时间
    __uint64_t nice_time;       //低优先级用户态运行时间
    __uint64_t system_time;     //内核态运行时间
    __uint64_t idle_time;       //空闲时间
    __uint64_t iowait_time;     //IO等待时间
    __uint64_t irq_time;        //硬件中断处理时间
    __uint64_t softirq_time;    //软件中断处理时间
    __uint64_t steal_time;      //虚拟化环境消耗时间
    __uint64_t guest_time;      //运行虚拟客户机时间
    __uint64_t guest_nice_time; //运行低优先级虚拟客户机时间
} cpu_time_t;

/* 系统信息 */
typedef struct {
    char name[32];
    char version[64];
    char arch[32];
} system_info_t;


/**
 * @brief  获取架构
 * 
 * @param  info  系统信息 
 * @return true  获取成功
 * @return false 获取失败
 */
bool get_arch(system_info_t *info);

/**
 * @brief  获取系统名字和版本
 * 
 * @param  info  系统信息 
 * @return true  获取成功
 * @return false 获取失败
 */
bool get_name_version(system_info_t *info);

/**
 * @brief  获取开机时间
 * 
 * @param  uptime 接收返回的开机时间
 * @return true   获取成功
 * @return false  获取失败
 */
bool get_uptime(float *uptime);

/**
 * @brief  获取 cpu 时间
 * 
 * @param  cpu_time 
 * @return true  获取成功
 * @return false 获取失败
 */
bool get_cpu_time(cpu_time_t *cpu_time);


/**
 * @brief 获取CPU 和 IO 的使用率
 * 
 * @param prev 前一次时间 
 * @param curr 当前时间
 * @param cpu_usage CPU 使用率
 * @param io_usage  IO 使用率
 */
void get_cpu_io_usage(const cpu_time_t *prev, const cpu_time_t *curr, float *cpu_usage, float *io_usage);


/**
 * @brief  获取内存使用率
 * 
 * @return float 内存使用率
 */
float get_mem_usage(void);

/**
 * @brief  获取硬盘使用率
 * 
 * @return float 
 */
float get_disk_usage(void);

/**
 * @brief 时间戳转换成时间字符串
 * 
 * @param time 用来存储转换的时间
 * @param timestamp 传入的时间戳
 */
void timestamp_to_timestr(char *time, time_t timestamp);

int main(void)
{
    int client_sockfd;
    char *sever_ip = "192.168.1.110";
    int port = 8081;
    struct sockaddr_in sever_addr;
    char cmd[512];
    system_info_t sys_info = {0};
    cpu_time_t prev = {0};
    cpu_time_t curr = {0};
    float uptime = 0.0;
    float cpu_usage = 0.0;
    float io_usage = 0.0;
    float mem_usage = 0.0;
    float disk_usage = 0.0;
    uint32_t timestamp = 0;
    char time[64];

    /* 创建一个 IPV4 TCP 的套结字 */
    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sockfd == -1) {
        printf("Failed to apply a socket\n");
        return -1;
    }

    /* 设置服务端地址 */
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_addr.s_addr = inet_addr(sever_ip);
    sever_addr.sin_port = htons(port);

    /* 发起连接请求 */
    int ret = connect(client_sockfd, (const struct sockaddr *)&sever_addr, sizeof(sever_addr));
    if (ret == -1) {
        printf("Failed to connect the sever\n");
        return -1;
    }

    while (1) {
        get_name_version(&sys_info);
        get_arch(&sys_info);
        get_uptime(&uptime);
        get_cpu_time(&curr);
        get_cpu_io_usage(&prev, &curr, &cpu_usage, &io_usage);
        prev = curr;
        mem_usage = get_mem_usage();
        disk_usage = get_disk_usage();

        encode_update_fstp_cmd(cmd, sys_info.name, sys_info.version, sys_info.arch, 
                                &uptime, &cpu_usage, &io_usage, &mem_usage, &disk_usage);
        ssize_t ret = send(client_sockfd, cmd, strlen(cmd), 0);
        if (ret < 0) {
            printf("Failed to send the cmd\n");
        } else {
            printf("Succeed to send: %s" ,cmd);
        }

        char recv_buf[256] = {0};
        ret = recv(client_sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (ret < 0) {
            printf("Failed to receive the cmd\n");
        } else if (ret == 0) {
            printf("tcp close\n");
        } else {
            if (is_fstp_complete(recv_buf)) {
                if (verify_fstp_time_cmd(recv_buf, &timestamp)) {
                    timestamp_to_timestr(time, (time_t)timestamp);
                }
            }
        }
        sleep(10);
    }

    close(client_sockfd);
    return 0;
}

bool get_arch(system_info_t *info)
{
    struct utsname sys_uname;
    if (uname(&sys_uname) == -1) {
        printf("Falied to get system arch\n");
        return false;
    }

    /* 打印获取信息 */
    printf("操作系统名称: %s\n", sys_uname.sysname);
    printf("网络节点名: %s\n", sys_uname.nodename);
    printf("操作系统发行版本: %s\n", sys_uname.release);
    printf("操作系统版本信息: %s\n", sys_uname.version);
    printf("硬件架构: %s\n", sys_uname.machine);

    /* 保留架构信息 */
    strncpy(info->arch, sys_uname.machine, sizeof(info->arch) - 1);
    info->arch[sizeof(info->arch) - 1] = '\0';

    return true;
}

bool get_name_version(system_info_t *info)
{
    char str[256]; //接受缓冲区
    FILE *fd = fopen("/etc/os-release", "r");
    if (fd == NULL) {
        printf("Failed to open the os-release\n");
        return false;
    }

    /* 获取信息 */
    while (fgets(str, sizeof(str) - 1, fd)) {
        if (strncmp(str, "NAME=", 5) == 0) {
            sscanf(str, "NAME=\"%31[^\"]\"", info->name);
        } else if (strncmp(str, "VERSION=", 8) == 0) {
            sscanf(str, "VERSION=\"%63[^\"]\"", info->version);
        } else {
            memset(str, 0, sizeof(str));
        }
    }
    fclose(fd);
    return true;
}

bool get_uptime(float *uptime)
{
    FILE *fd = fopen("/proc/uptime", "r");
    if (fd == NULL) {
        printf("Failed to open the uptime\n");
        return false;
    }

    fscanf(fd, "%f", uptime);
    fclose(fd);

    return true;
}

bool get_cpu_time(cpu_time_t *cpu_time)
{
    FILE *fd = fopen("/proc/stat", "r");
    if (fd == NULL) {
        printf("Failed to open the stat\n");
        return false;
    }

    int ret = fscanf(fd, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
        &cpu_time->user_time, &cpu_time->nice_time, &cpu_time->system_time, &cpu_time->idle_time,
        &cpu_time->iowait_time, &cpu_time->irq_time, &cpu_time->softirq_time, 
        &cpu_time->steal_time, &cpu_time->guest_time, &cpu_time->guest_nice_time);

    if (ret != 10) 
        return false;
    return true;
}

void get_cpu_io_usage(const cpu_time_t *prev, const cpu_time_t *curr, float *cpu_usage, float *io_usage)
{
    __int64_t user_diff = curr->user_time - prev->user_time; 
    __int64_t nice_diff = curr->nice_time - prev->nice_time;
    __int64_t system_diff = curr->system_time - prev->system_time;
    __int64_t idle_diff = curr->idle_time - prev->idle_time;
    __int64_t iowait_diff = curr->iowait_time - prev->iowait_time;
    __int64_t irq_diff = curr->irq_time - prev->irq_time;
    __int64_t softirq_diff = curr->softirq_time - prev->softirq_time;
    __int64_t steal_diff = curr->steal_time - prev->steal_time;
    __int64_t guest_diff = curr->guest_time - prev->guest_time;
    __int64_t guest_nice_diff = curr->guest_nice_time - prev->guest_nice_time;
    
    if (user_diff < 0 || nice_diff < 0 || system_diff < 0|| idle_diff < 0 ||
        iowait_diff < 0 || irq_diff < 0 || softirq_diff < 0 || steal_diff < 0 ||
        steal_diff < 0 || guest_diff < 0 || guest_nice_diff < 0) {
            printf("The time is illogical\n");
    }

    __uint64_t total_diff = user_diff + nice_diff + system_diff + idle_diff + iowait_diff +
                            irq_diff + softirq_diff + steal_diff + steal_diff + guest_diff + guest_nice_diff;
    
    *cpu_usage = (float)(total_diff - idle_diff) / (float)total_diff * 100.0;
    *io_usage = (float)iowait_diff / (float)total_diff * 100.0;
    printf("CPU usage: %.2f %%, IO usage: %.2f %%\n", *cpu_usage, *io_usage);
}

float get_mem_usage(void)
{
    unsigned long total, free, available = 0;
    float mem_usage = 0.0;
    FILE *fd = fopen("/proc/meminfo", "r");
    if (fd == NULL) {
        printf("Failed to open the meminfo\n");
        return 0.0;
    }
    /* 提取信息 */
    char line[256] = {0};
    while (fgets(line, sizeof(line) - 1, fd)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %lu kB", &total);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %lu kB", &free);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %lu kB", &available);
        } else { 
            memset(line, 0, sizeof(line));
        }
    }
    fclose(fd);
    
    printf("Total: %lu, Free: %lu, Available: %lu\n", total, free, available);

    if (total && available) {
        mem_usage = (float)(total - available) / (float)total * 100.0;
    } else if (total && free) {
        mem_usage = (float)(total - free) / (float)total * 100.0;
    }

    printf("MEM usage: %.2f %%\n", mem_usage);
    return mem_usage;
}

float get_disk_usage(void)
{
    struct statvfs sys_statvfs;
    float disk_usage = 0.0;

    if (statvfs("/", &sys_statvfs) == -1) { //获取系统总磁盘信息
        printf("Failed to get the system disk use info\n");
        return 0.0;
    }

    disk_usage = (float)(sys_statvfs.f_blocks - sys_statvfs.f_bfree) / (float)sys_statvfs.f_blocks * 100.0;
    printf("DISK usage: %.2f %%\n", disk_usage);
    return disk_usage;
}

void timestamp_to_timestr(char *time, time_t timestamp)
{
    struct tm *local_time = localtime(&timestamp);

    sprintf(time, "Time: %04d-%02d-%02d %02d:%02d:%02d\n", local_time->tm_year + 1900, 
        local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

    printf("%s", time);
}

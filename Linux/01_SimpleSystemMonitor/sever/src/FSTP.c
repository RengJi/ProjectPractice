#include <FSTP.h>

/**
 * @brief  用来检查是否是一条完整的 fstp 协议指令
 * 
 * @param  str 传入的 fstp 协议指令
 * @return bool false 代表不完整；true 代表是一条完整指令
 */
bool is_fstp_complete(const char *str)
{
        __uint8_t len = strlen(str);
        if (len < 2)  //长度不够
                return false;
        
        return (str[len-1] == '\n' && str[len-2] == '\r');
}

/**
 * @brief  验证传入的 update 指令是否正确
 * 
 * @param  cmd 传入的指令 
 * @return bool false 代表不正确，true 代表正确
 */
bool verify_fstp_update_cmd(const char *cmd)
{
        /* 自定义文本协议， 示例如下：UPDATE Name,Ubuntu,Version,18.04.6 LTS(Bionic Beaver),
        Arch,x86_64,Uptime,58328.03,CPU,0.23,IO,0.11,Memory,51.77,Disk,20.42\r\n*/
        __uint8_t ret;
        char name[32];
        char version[64];
        char arch[32];
        float uptime;
        float cpu_usage;
        float io_usage;
        float mem_usage;
        float disk_usage;
        
        ret = sscanf(cmd, "UPDATE Name, %31[^,],Version, %63[^,],Arch,%31[^,],Uptime,%f,CPU,%f,IO,%f,Memory,%f,Disk,%f\r\n", 
                        name, version, arch, &uptime, &cpu_usage, &io_usage, &mem_usage, &disk_usage);
        
        if (ret != 8) 
                return false;

        return true;
}

/**
 * @brief  验证 TIME 命令格式并且提取时间戳
 * 
 * @param  cmd 传入的命令 
 * @param  timestamp 获取返回的时间戳
 * @return bool false 代表不正确，true 代表正确
 */
bool verify_fstp_time_cmd(const char *cmd, __uint32_t *timestamp)
{
        return (sscanf(cmd, "TIME %u", timestamp) == 1);
}

/**
 * @brief  编码成FSTP命令格式
 * 
 * @param  cmd  传入的命令
 * @param  name 系统名称
 * @param  version 系统版本
 * @param  uptime  设备开机时间
 * @param  arch  设备架构
 * @param  cpu_usage cpu使用率
 * @param  io_usage  io使用率
 * @param  mem_usage mem使用率
 * @param  dsik_usage disk使用率
 * @return 无
 */
void encode_update_fstp_cmd(char *cmd, char *name, char *version, char *arch, float *uptime, 
        float *cpu_usage, float *io_usage, float *mem_usage, float *disk_usage)
{
        sprintf(cmd, "UPDATE Name, %s,Version, %s,Arch,%s,Uptime,%.2f,CPU,%.2f,IO,%.2f,Memory,%.2f,Disk,%.2f\r\n",
                name, version, arch, *uptime, *cpu_usage, *io_usage, *mem_usage, *disk_usage);
}
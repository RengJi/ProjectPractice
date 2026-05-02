#ifndef __FSTP_H__
#define __FSTP_H__
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* 自定义文本协议， 示例如下：UPDATE Name,Ubuntu,Version,18.04.6 LTS(Bionic Beaver),
Arch,x86_64,Uptime,58328.03,CPU,0.23,I0,0.11,Memory,51.77,Disk,20.42\r\n*/

/**
 * @brief  用来检查是否是一条完整的 fstp 协议指令
 * 
 * @param  str 传入的 fstp 协议指令
 * @return bool false 代表不完整；true 代表是一条完整指令
 */
bool is_fstp_complete(const char *str);

/**
 * @brief  验证传入的 update 指令是否正确
 * 
 * @param  cmd 传入的指令 
 * @return bool false 代表不正确，true 代表正确
 */
bool verify_fstp_update_cmd(const char *cmd);

/**
 * @brief  验证 TIME 命令格式并且提取时间戳
 * 
 * @param  cmd 传入的命令 
 * @param  timestamp 获取返回的时间戳
 * @return bool false 代表不正确，true 代表正确
 */
bool verify_fstp_time_cmd(const char *cmd, __uint32_t *timestamp);

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
        float *cpu_usage, float *io_usage, float *mem_usage, float *disk_usage);

#endif
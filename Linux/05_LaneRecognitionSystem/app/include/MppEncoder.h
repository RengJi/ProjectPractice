#pragma once

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_mpi_cmd.h>
#include <vector>
#include <mutex>
#include "DataType.h"

#define MPP_ENC_DEFAULT_CODINGTYEP MPP_VIDEO_CodingAVC // 默认编码类型为H.264/AVC


namespace MppEncPacketStatus
{
    inline constexpr int HasPacket = 0;
    inline constexpr int NoPacket = 1;
    inline constexpr int Error = -1;
}

// MPP编码器输出结果结构体
struct MppEncPacketResult
{
    const void *data = nullptr; // 编码数据指针
    size_t size = 0;            // 编码数据大小
    int64_t dtsUs = -1;         // 解码时间戳（
    int64_t ptsUs = -1;         // 显示时间戳（微秒）
    bool eos = false;           // 流结束标志
    bool eoi = false;           // 图像结束标志
    bool isPartition = false;   // 是否为分片数据
    bool isExtraData = false;   // 是否为额外数据（如SPS/PPS）
    void *handle = nullptr;     // 内部句柄用于资源回收
};

// MPP编码器类
class MppEncoder 
{
public:
    MppEncoder();
    ~MppEncoder();

    int Init();
    int SetWidthHeight(uint32_t width, uint32_t height, uint32_t hor_stride, uint32_t ver_stride);
    int PutFrame(const IoFd *io);
    int GetPacket(MppEncPacketResult &result);
    int ReleasePacket(MppEncPacketResult &result);
    bool IsPacketEos() { return packetEos; }
private:
    MppCtx mppCtx = nullptr;  
    MppApi *mppApi = nullptr;  
    MppEncPrepCfg prepCfg{};    // 预处理配置
    MppEncRcCfg rcCfg{};        // 码率控制配置
    MppEncCodecCfg codecCfg{};  // 编码器配置

    uint32_t width = 0;     // 视频宽度
    uint32_t height = 0;    // 视频高度
    uint32_t horStride = 0; // 水平步幅
    uint32_t verStride = 0; // 垂直步幅
    uint32_t fps = 30;      // 视频帧率
    uint32_t bitRate = 2000000; // 视频码率
    uint32_t gop = 60;      // 关键帧间隔

    bool packetEos = false; // 数据包结束标志
    std::mutex encMutex; 
};
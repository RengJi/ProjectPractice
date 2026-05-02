#pragma once

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_mpi_cmd.h>
#include <vector>
#include <deque>
#include <array>
#include <mutex>
#include <map>
#include "DataType.h"

#define DEFAULT_MPP_DECODER_TYPE MPP_VIDEO_CodingAVC // 默认使用H.264编码类型
#define NEED_SPLIT  1   

class MppDecoder
{
private:
    struct DecodeTaskHolder // 解码任务持有者结构体
    {
        IoFd *outputDesc = nullptr;
        MppFrame outFrame = nullptr;
        MppBuffer outBuffer = nullptr;
    };
    
    int AllocBufDmaFd(IoFd *desc, size_t size); 
    //int CommitExternalOutputBuffer(size_t size);
    void ReleaseExternalOutputBuffer();
    void ReleaseTaskPacket(MppTask task);
    DecodeTaskHolder* GetAvailableHolder();
    void ReturnHolder(DecodeTaskHolder* holder);
    void RecycleTaskHolder(DecodeTaskHolder* holder);
    void DrainPendingTasks();
    void ForcePendingTasks();
public:
    MppDecoder();
    ~MppDecoder();
    
    int Init();
    int SetWidthHeight(uint32_t width, uint32_t height);
    int AllocInputBuffers(const FrameDescription *frame_desc_array, size_t buffer_count);
    int Decode(const FrameDescription *frame_desc);
    int QueueOutputForRecycle(const IoFd *output_desc);
private:
    MppCtx *ctx = nullptr;
    MppApi *mpi = nullptr;
    MppBufferGroup *group = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t horStride = 0;  // 水平步长
    uint32_t verStride = 0;  // 垂直步长
    size_t outSize = 0;

    std::mutex taskMutex; 
    IoFd mppOutputFdList[ResourceLimit::MppOutputBufferCount] = {};                 // MPP输出文件描述符列表
    std::map<int, size_t> fdToIndexMap;                                             // 文件描述符到索引的映射
    std::array<DecodeTaskHolder, ResourceLimit::MppOutputBufferCount> holderPool;   // 解码任务持有者池
    std::deque<DecodeTaskHolder*> availableHolders;                                 // 可用解码任务持有者队列
    std::deque<DecodeTaskHolder*> recycleHolders;                                   // 待回收的解码任务持有者队列
    std::map<const IoFd*, DecodeTaskHolder*> fdToHolderMap;                         // 输入文件描述符到解码任务持有者的映射
public:
    MppBuffer importBuffers[ResourceLimit::MppImportBufferCount] = {};              // MPP导入缓冲区列表
    const IoFd* currentOutputDesc = nullptr;                                        // 当前输出描述符指针
};
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <error.h>
#include <linux/dma-heap.h>
#include "MppDecoder.h"
#include "Logger.hpp"

/**
 * @brief 分配DMA缓冲区并获取文件描述符
 * @param desc 指向IoFd结构体的指针，用于存储分配的DMA缓冲区信息
 * @param size 需要分配的DMA缓冲区大小
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::AllocBufDmaFd(IoFd *desc, size_t size)
{
    // 1. 打开dma_heap设备文件以获取文件描述符
    int fd = open("dev/dma_heap/system", O_RDWR);
    if (fd < 0)
    {
        LOG_ERROR("MPP_DEC", "Failed to open dma_heap: ", strerror(errno));
        return -1;
    }

    // 2. 使用ioctl调用DMA_HEAP_IOCTL_ALLOC命令分配DMA缓冲区，并获取分配的文件描述符
    struct dma_heap_allocation_data allocData = {};
    allocData.len = size;
    allocData.fd_flags = O_RDWR | O_CLOEXEC; // 确保分配的文件描述符具有正确的权限和标志
    if (ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &allocData) < 0)
    {
        LOG_ERROR("MPP_DEC", "Failed to allocate DMA buffer: ", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd); // 关闭dma_heap的文件描述符，allocData.fd是分配的DMA缓冲区的文件描述符

    desc->fd = allocData.fd;
    desc->size = size;
    return 0;
}

/*
int MppDecoder::CommitExternalOutputBuffer(size_t size)
{
    // TODO: 实现提交外部输出缓冲区的逻辑
    return 0;
}
*/

/**
 * @brief 释放所有已提交的外部输出缓冲区
 * @return 无返回值
 */
void MppDecoder::ReleaseExternalOutputBuffer()
{
    for (size_t i = 0; i < ResourceLimit::MppOutputBufferCount; ++i)
    {
        if (mppOutputFdList[i].fd >= 0)
        {
            close(mppOutputFdList[i].fd);
            mppOutputFdList[i] = {};
        }
    }
}

/**
 * @brief 释放解码任务使用的输入包，并将任务结构体返回给MPP队列
 * @param task 需要释放的解码任务
 * @return 无返回值
 */
void MppDecoder::ReleaseTaskPacket(MppTask task)
{
    if (!task)
    {
        return;
    }

    // 1. 获取输入包并释放
    MppPacket packet = nullptr;
    mpp_task_meta_get_packet(task, KEY_INPUT_PACKET, &packet);
    if (packet)
    {
        mpp_packet_deinit(&packet);
    }

    // 2. 回收任务结构体，返回可以队列
    mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
}

/**
 * @brief 从解码任务持有者池中获取一个可用的持有者
 * @return 成功返回指向可用DecodeTaskHolder的指针，失败返回nullptr
 */
MppDecoder::DecodeTaskHolder *MppDecoder::GetAvailableHolder()
{
    std::lock_guard<std::mutex> lock(taskMutex);
    if (availableHolders.empty())
    {
        LOG_ERROR("MPP_DEC", "No available DecodeTaskHolder in the pool");
        return nullptr;
    }
    DecodeTaskHolder *holder = availableHolders.front();
    availableHolders.pop_front();
    return holder;
}

/**
 * @brief 将解码任务持有者返回到可用队列中，并进行必要的清理
 * @param holder 需要返回的DecodeTaskHolder指针
 * @return 无返回值
 */
void MppDecoder::ReturnHolder(DecodeTaskHolder *holder)
{
    if (holder->outFrame)
    {
        mpp_frame_deinit(&holder->outFrame);
        holder->outFrame = nullptr;
    }
    holder->outputDesc = nullptr;
    holder->outBuffer = nullptr;

    std::lock_guard<std::mutex> lock(taskMutex);
    availableHolders.push_back(holder);
}

/**
 * @brief 将解码任务持有者放入待回收队列中，等待后续处理
 * @param holder 需要回收的DecodeTaskHolder指针
 * @return 无返回值
 */
void MppDecoder::RecycleTaskHolder(DecodeTaskHolder *holder)
{
    std::lock_guard<std::mutex> lock(taskMutex);
    recycleHolders.push_back(holder);
}

/**
 * @brief 清空所有待回收的解码任务持有者，将它们返回到可用队列中
 * @return 无返回值
 */
void MppDecoder::DrainPendingTasks()
{
    std::deque<DecodeTaskHolder *> pending;
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        pending.swap(recycleHolders); // 交换队列，减少锁持有时间
    }

    for (auto *holder : pending)
    {
        // 这里可以添加一些日志记录，帮助调试和监控
        LOG_INFO("MPP_DEC", "Draining pending task for output fd: ", holder->outputDesc ? holder->outputDesc->fd : -1);
        ReturnHolder(holder);
    }
}

/**
 * @brief 强制处理所有待处理的解码任务，将它们返回到可用队列中，并清空输入文件描述符到持有者的映射
 * @return 无返回值
 */
void MppDecoder::ForcePendingTasks()
{
    std::lock_guard<std::mutex> lock(taskMutex);
    for (auto &kv : fdToHolderMap)
    {
        ReturnHolder(kv.second);
    }
    fdToHolderMap.clear();
}

MppDecoder::MppDecoder() = default;

MppDecoder::~MppDecoder()
{
    ForcePendingTasks();
    DrainPendingTasks();
    ReleaseExternalOutputBuffer();

    if (ctx)
    {
        mpi->reset(ctx);
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
    }

    if (group)
    {
        mpp_buffer_group_put(group);
        group = nullptr;
    }

    LOG_INFO("MPP_DEC", "MppDecoder destroyed and resources released");
}

/**
 * @brief 初始化MPP解码器，创建和配置MPP上下文，并准备输出缓冲区
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::Init()
{
    MppCodingType codingType = DEFAULT_MPP_DECODER_TYPE;

    if (mpp_create(ctx, &mpi) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to create MPP context");
        return -1;
    }

    if (mpp_init(ctx, MPP_CTX_DEC, codingType) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to initialize MPP context for decoding");
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
        return -1;
    }

    MppDecCfg decCfg = {};
    mpp_dec_cfg_init(&decCfg);

    int needSplit = NEED_SPLIT;
    mpp_dec_cfg_set_s32(&decCfg, "split_parse_mode", needSplit); // 设置是否需要分割输入数据

    if (mpi->control(ctx, MPP_DEC_SET_CFG, &decCfg) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to set decoder configuration");
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
        return -1;
    }

    mpp_dec_cfg_deinit(&decCfg);

    if (mpp_buffer_group_get_internal(group, MPP_BUFFER_TYPE_ION) != MPP_OK) // 这个地方内存格式可以根据需要调整
    {
        LOG_ERROR("MPP_DEC", "Failed to get internal buffer group");
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
        return -1;
    }

    for (size_t i = 0; i < ResourceLimit::MppOutputBufferCount; ++i)
    {
        availableHolders.push_back(&holderPool[i]);
    }

    LOG_INFO("MPP_DEC", "MppDecoder initialized successfully");
    return 0;
}

/**
 * @brief 设置解码器的宽度和高度，并配置相关参数
 * @param width 解码器输出图像的宽度
 * @param height 解码器输出图像的高度
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::SetWidthHeight(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    horStride = (width + 15) & ~15;  // 对齐到16的倍数
    verStride = (height + 15) & ~15; // 对齐到16的倍数
    outSize = horStride * verStride * 3 / 2;

    MppDecCfg decCfg = {};
    mpp_dec_cfg_init(&decCfg);
    mpp_dec_cfg_set_u32(&decCfg, "width", width);
    mpp_dec_cfg_set_u32(&decCfg, "height", height);
    mpp_dec_cfg_set_u32(&decCfg, "hor_stride", horStride);
    mpp_dec_cfg_set_u32(&decCfg, "ver_stride", verStride);

    mpp_dec_cfg_set_s32(&decCfg, "format", MPP_FMT_YUV420SP); // 设置输出格式为NV12

    if (mpi->control(ctx, MPP_DEC_SET_CFG, &decCfg) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to set decoder configuration for width and height");
        mpp_dec_cfg_deinit(&decCfg);
        return -1;
    }

    mpp_dec_cfg_deinit(&decCfg);
    LOG_INFO("MPP_DEC", "Decoder width and height set to %ux%u with stride %ux%u", width, height, horStride, verStride);
    return 0;
}

/**
 * @brief 分配输入缓冲区并将其导入到MPP缓冲区组中，以供后续解码使用
 * @param frame_desc_array 指向FrameDescription结构体数组的指针，包含需要分配的输入缓冲区信息
 * @param buffer_count 输入缓冲区的数量
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::AllocInputBuffers(const FrameDescription *frame_desc_array, size_t buffer_count)
{
    if (buffer_count == 0 || !frame_desc_array)
    {
        LOG_ERROR("MPP_DEC", "Invalid input buffer description");
        return -1;
    }

    for (size_t i = 0; i < ResourceLimit::MppImportBufferCount && i < buffer_count; ++i)
    {
        const FrameDescription &desc = frame_desc_array[i];
        if (desc.fd < 0 || desc.base == nullptr || desc.length == 0)
        {
            LOG_ERROR("MPP_DEC", "Invalid frame description at index ", i);
            return -1;
        }

        MppBufferInfo_t info{};
        info.fd = desc.fd;
        info.size = desc.length;
        info.ptr = desc.base;

        if (mpp_buffer_import(group, &info) != MPP_OK)  // 将输入缓冲区导入到MPP缓冲区组中
        {
            LOG_ERROR("MPP_DEC", "Failed to import input buffer at index ", i);
            return -1;
        }
    }

    LOG_INFO("MPP_DEC", "Successfully allocated and imported ", buffer_count, " input buffers");
    return 0;
}

/**
 * @brief 解码输入帧描述，并将解码后的输出帧准备好供外部访问
 * @param frame_desc 指向FrameDescription结构体的指针，包含需要解码的输入帧信息
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::Decode(const FrameDescription *frame_desc)
{
    if (!frame_desc || frame_desc->fd < 0 || frame_desc->base == nullptr || frame_desc->length == 0)
    {
        LOG_ERROR("MPP_DEC", "Invalid frame description for decoding");
        return -1;
    }

    // 1. 构建输入数据包
    MppPacket packet = nullptr;
    void *pktData = frame_desc->base;         // 直接使用有效载荷数据的内存地址
    size_t pktSize = frame_desc->payloadSize; // 使用有效载荷大小而不是整个缓冲区长度

    if (mpp_packet_init_with_buffer(&packet, importBuffers[frame_desc->index]) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to initialize MPP packet with input buffer");
        return -1;
    }

    mpp_packet_set_data(packet, pktData);
    mpp_packet_set_size(packet, pktSize);
    mpp_packet_set_pts(packet, frame_desc->ptsUs);
    mpp_packet_set_dts(packet, frame_desc->dtsUs);
    mpp_packet_set_length(packet, pktSize); 

    // 2. 获取输入任务并将数据包设置到任务元数据中
    MppTask task = nullptr;
    if (mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK) != MPP_OK) // 等待输入任务可用
    {
        LOG_ERROR("MPP_DEC", "Failed to poll for input task");
        mpp_packet_deinit(&packet);
        return -1;
    }

    if (mpi->dequeue(ctx, MPP_PORT_INPUT, &task) != MPP_OK || !task) // 获取输入任务
    {
        LOG_ERROR("MPP_DEC", "Failed to dequeue input task");
        mpp_packet_deinit(&packet);
        return -1;
    }

    if (mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet) != MPP_OK) // 将数据包设置到任务元数据中
    {
        LOG_ERROR("MPP_DEC", "Failed to set input packet to task metadata");
        mpp_packet_deinit(&packet);
        mpi->enqueue(ctx, MPP_PORT_INPUT, task); // 任务入队以避免泄漏
        return -1;
    }

    if (mpi->enqueue(ctx, MPP_PORT_INPUT, task) != MPP_OK) // 将任务重新入队
    {
        LOG_ERROR("MPP_DEC", "Failed to enqueue input task");
        mpp_packet_deinit(&packet);
        return -1;
    }

    // 3. 处理输出任务，获取解码后的帧并准备输出描述符
    if (mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_NON_BLOCK) != MPP_OK) // 尝试非阻塞方式获取输出任务
    {
        LOG_ERROR("MPP_DEC", "Failed to poll for output task");
        return -1;
    }

    task = nullptr;
    if (mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task) != MPP_OK || !task) // 获取输出任务
    {
        LOG_ERROR("MPP_DEC", "Failed to dequeue output task");
        return -1;
    }

    MppFrame outFrame = nullptr;

    if (mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &outFrame) != MPP_OK || !outFrame) // 从任务元数据中获取输出帧
    {
        LOG_ERROR("MPP_DEC", "Failed to get output frame from task metadata");
        ReleaseTaskPacket(task); 
        return -1;
    }

    MppBuffer outBuffer  = mpp_frame_get_buffer(outFrame); // 获取输出帧的缓冲区    
    if (!outBuffer)
    {
        LOG_ERROR("MPP_DEC", "Failed to get output buffer from frame");
        ReleaseTaskPacket(task);
        mpp_frame_deinit(&outFrame);
        return -1;
    }

    // 4. 获取一个可用的DecodeTaskHolder来持有当前的输出任务信息
    DecodeTaskHolder *holder = GetAvailableHolder();
    if (!holder)
    {
        LOG_ERROR("MPP_DEC", "No available DecodeTaskHolder for output task");
        ReleaseTaskPacket(task);
        mpp_frame_deinit(&outFrame);
        return -1;
    }

    MppBufferInfo_t outBufInfo;
    if (mpp_buffer_info_get(outBuffer, &outBufInfo) != MPP_OK)
    {
        LOG_ERROR("MPP_DEC", "Failed to get output buffer info");
        ReturnHolder(holder);
        ReleaseTaskPacket(task);
        mpp_frame_deinit(&outFrame);
        return -1;
    }

    // 5. 将输出帧和缓冲区信息保存到DecodeTaskHolder中，并准备输出描述符
    holder->outFrame = outFrame;
    holder->outBuffer = outBuffer;

    holder->outputDesc = &mppOutputFdList[holder - holderPool.data()]; // 计算对应的输出文件描述符
    holder->outputDesc->fd = outBufInfo.fd;
    holder->outputDesc->base = outBufInfo.ptr;
    holder->outputDesc->size = outBufInfo.size;
    holder->outputDesc->format = MPP_FMT_YUV420SP; // 输出格式
    holder->outputDesc->width = width;
    holder->outputDesc->height = height;
    holder->outputDesc->horStride = horStride;
    holder->outputDesc->verStride = verStride;
    holder->outputDesc->dtsUs = mpp_frame_get_dts(outFrame);
    holder->outputDesc->ptsUs = mpp_frame_get_pts(outFrame);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        fdToHolderMap[holder->outputDesc] = holder; // 将输出文件描述符与持有者关联
    }

    currentOutputDesc = holder->outputDesc; // 更新当前输出描述符指针，供外部访问
    
    ReleaseTaskPacket(task); // 释放输入包并将任务结构体返回给MPP队列
    LOG_INFO("MPP_DEC", "Decoded frame with output fd: ", holder->outputDesc->fd, ", pts: ", holder->outputDesc->ptsUs, " us, dts: ", holder->outputDesc->dtsUs, " us");
    return 0;   
}

/**
 * @brief 将解码完成的输出缓冲区加入回收队列，等待后续处理
 * @param output_desc 指向IoFd结构体的指针，包含需要回收的输出缓冲区信息
 * @return 成功返回0，失败返回-1
 */
int MppDecoder::QueueOutputForRecycle(const IoFd *output_desc)
{
    if (!output_desc)
    {
        LOG_ERROR("MPP_DEC", "Invalid output descriptor for recycling");
        return -1;
    }

    std::lock_guard<std::mutex> lock(taskMutex);
    auto it = fdToHolderMap.find(output_desc);
    if (it == fdToHolderMap.end())
    {
        LOG_ERROR("MPP_DEC", "No DecodeTaskHolder found for output fd: ", output_desc->fd);
        return -1;
    }
    DecodeTaskHolder *holder = it->second;
    recycleHolders.push_back(holder);
    fdToHolderMap.erase(it);

    LOG_INFO("MPP_DEC", "Queued output fd ", output_desc->fd, " for recycling");
    return 0;
}
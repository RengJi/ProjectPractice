#include "RgaProcess.h"
#include "Logger.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <error.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <rga/im2d.h>
#include <rga/RgaApi.h>
#include <linux/dma-heap.h>

const char *dmaBufPath = "/dev/dma_heap/system"; // dma 内存控制文件

/**
 * @brief 初始化 dma 内存
 * @param output 输出缓冲区信息结构体指针
 * @param size 需要申请的 dma 内存大小
 * @return 0 成功，-1 失败
 */
int RgaProcessor::InitDmaBuf(IoFd *output, size_t size)
{
    // 1. 打开 dma 控制文件
    int fd = open(dmaBufPath, O_RDWR);
    if (fd < 0)
    {
        LOG_ERROR("RGA", "Failed to open dma heap device: ", strerror(errno));
        return -1;
    }

    // 2. 申请 dma 内存
    struct dma_heap_allocation_data allocData{};
    allocData.len = size;
    allocData.fd_flags = O_RDWR | O_CLOEXEC;

    if (ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &allocData) < 0)
    {
        LOG_ERROR("RGA", "Failed to allocate dma buffer: ", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);

    // 3. 将 dma 内存映射到用户空间
    output->fd = allocData.fd;
    output->size = size;
    output->allocSize = size;
    output->base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, output->fd, 0);
    if (output->base == MAP_FAILED)
    {
        LOG_ERROR("RGA", "Failed to mmap dma buffer: ", strerror(errno));
        close(output->fd);
        output->fd = -1;
        return -1;
    }
    LOG_INFO("RGA", "DMA buffer allocated: fd=", output->fd, ", size=", size, ", base=", output->base);

    return 0;
}

/**
 * @brief 释放 dma 内存
 * @param output 输出缓冲区信息结构体指针
 * @return 无返回值
 */
void RgaProcessor::ReleaseDmaBuf(IoFd *output)
{
    if (output->base && output->base != MAP_FAILED)
    {
        munmap(output->base, output->size);
        output->base = nullptr;
    }
    if (output->fd >= 0)
    {
        close(output->fd);
        output->fd = -1;
    }
    output->size = 0;
}

/**
 * @brief 检查图像配置是否有效
 * @param config 图像配置结构体
 * @return true 配置有效，false 配置无效
 */
bool RgaProcessor::IsConfigValid(const RgaProcessor::ImageConfig &config) const
{
    return config.valid && config.width > 0 && config.height > 0 && config.horStride >= config.width && config.verStride >= config.height;
}

/**
 * @brief 从输入缓冲区加载图像配置
 * @param io 输入缓冲区信息结构体
 * @param config 输出图像配置结构体
 * @return true 加载成功且配置有效，false 加载失败或配置无效
 */
bool RgaProcessor::LoadConfig(const IoFd &io, RgaProcessor::ImageConfig &config)
{
    config.width = io.width;
    config.height = io.height;
    config.horStride = io.horStride;
    config.verStride = io.verStride;
    config.format = io.format;
    config.valid = io.width > 0 && io.height > 0 && io.horStride >= io.width && io.verStride >= io.height;
    return config.valid;
}

/**
 * @brief 初始化 RGA 输出缓冲区
 * @param config 图像配置结构体
 * @return 0 成功，-1 失败
 */
int RgaProcessor::InitOutputBuffers(const RgaProcessor::ImageConfig &config)
{
    if (outReady)
    {
        return 0;
    }

    if (!IsConfigValid(config))
    {
        LOG_ERROR("RGA", "Invalid image config for output buffers");
        return -1;
    }

    // 1. 根据图像配置计算每个输出缓冲区所需的内存大小
    size_t bufferSize = CalcNv12ImageSize(config.horStride, config.verStride);
    for (size_t i = 0; i < ResourceLimit::RgaOutputBufferCount; ++i)
    {
        // 初始化每个输出缓冲区的 dma 内存
        if (InitDmaBuf(&outBuffers[i], bufferSize) < 0)
        {
            LOG_ERROR("RGA", "Failed to initialize output buffer ", i);
            ReleaseOutputBuffers();
            return -1;
        }

        // 设置输出缓冲区的图像配置信息
        outBuffers[i].width = config.width;
        outBuffers[i].height = config.height;
        outBuffers[i].horStride = config.horStride;
        outBuffers[i].verStride = config.verStride;
        outBuffers[i].format = config.format;
        outBuffers[i].allocSize = bufferSize;

        outBufferInUse[i] = false;
        availablePositions.push(&outBuffers[i]);
    }
    // 2. 标记输出缓冲区准备就绪
    outReady = true;
    LOG_INFO("RGA", "Initialized ", ResourceLimit::RgaOutputBufferCount, " output buffers with size ", bufferSize, " bytes each");
    return 0;
}

/**
 * @brief 释放 RGA 输出缓冲区
 * @return 无返回值
 */
void RgaProcessor::ReleaseOutputBuffers()
{
    for (size_t i = 0; i < ResourceLimit::RgaOutputBufferCount; ++i)
    {
        ReleaseDmaBuf(&outBuffers[i]);
        outBufferInUse[i] = false;
    }

    while (!availablePositions.empty())
    {
        availablePositions.pop();
    }
    outReady = false;
}

/**
 * @brief 检查给定缓冲区指针是否属于输出缓冲区成员
 * @param buffer 待检查的缓冲区指针
 * @return true 是输出缓冲区成员，false 不是输出缓冲区成员
 */
bool RgaProcessor::IsOutputMember(const IoFd *buffer) const
{
    if (buffer == nullptr)
    {
        LOG_ERROR("RGA", "Null buffer pointer passed to IsOutputMember");
        return false;
    }

    ptrdiff_t diff = buffer - outBuffers;
    if (diff < 0 || diff >= static_cast<ptrdiff_t>(ResourceLimit::RgaOutputBufferCount))
    {
        LOG_ERROR("RGA", "Buffer pointer ", buffer, " is out of range for output buffers");
        return false;
    }

    LOG_INFO("RGA", "Buffer pointer ", buffer, " is a valid output member at index ", diff);
    return true;
}

/**
 * @brief 获取一个可用的输出缓冲区
 * @return 指向可用输出缓冲区的指针，如果没有可用缓冲区则返回 nullptrs
 */
IoFd *RgaProcessor::AquireOutputBuffer()
{
    std::lock_guard<std::mutex> lock(outMtx);
    if (availablePositions.empty())
    {
        LOG_WARNING("RGA", "No available output buffers to acquire");
        return nullptr;
    }

    IoFd *buffer = availablePositions.front();
    availablePositions.pop();

    size_t index = buffer - outBuffers;
    if (index >= ResourceLimit::RgaOutputBufferCount)
    {
        LOG_ERROR("RGA", "Acquired buffer pointer ", buffer, " is out of range for output buffers");
        return nullptr;
    }
    outBufferInUse[index] = true;

    LOG_INFO("RGA", "Acquired output buffer at index ", index);
    return buffer;
}

/**
 * @brief 内部执行 RGA 变换操作的函数
 * @param src 输入缓冲区信息结构体指针
 * @param op 需要执行的 RGA 操作类型
 * @param dst_width 目标图像宽度（仅在 Resize 操作时使用）
 * @param dst_height 目标图像高度（仅在 Resize 操作时使用）
 * @param dst_format 目标图像格式（仅在 ConvertFormat 操作时使用）
 */
IoFd *RgaProcessor::TransformInternal(const IoFd *src, RgaProcessor::Operation op, uint32_t dst_width, uint32_t dst_height, uint32_t dst_format)
{
    if (!initialized || !src)
    {
        LOG_ERROR("RGA", "Invalid state or source buffer for TransformInternal");
        return nullptr;
    }

    // 1. 获取输入配置并且通过配置设置输出缓冲区配置，确保输出缓冲区已正确初始化
    ImageConfig srcConfig;
    if (!LoadConfig(*src, srcConfig)) // 加载源图像配置并验证有效性
    {
        LOG_ERROR("RGA", "Failed to load source image config for transformation");
        return nullptr;
    }

    if (InitOutputBuffers(srcConfig) < 0) // 确保输出缓冲区已初始化且配置正确
    {
        LOG_ERROR("RGA", "Failed to initialize output buffers for transformation");
        return nullptr;
    }

    IoFd *dst = AquireOutputBuffer(); // 获取一个可用的输出缓冲区用于存储变换结果
    if (!dst)
    {
        LOG_ERROR("RGA", "Failed to acquire output buffer for transformation");
        return nullptr;
    }

    // 2. 根据输入配置和操作类型准备 RGA 输入输出缓冲区结构体，并决定使用 fd 模式还是虚拟地址模式进行数据传输
    // 准备 RGA 输入输出缓冲区结构体
    // 关键：librga 要求两边要么都有 handle，要么都没有
    // 如果源 buffer 有 DMA-BUF fd，两边都用 fd；否则两边都用虚拟地址
    int srcFormat = (srcConfig.format == MPP_FMT_YUV420SP ? RK_FORMAT_YCbCr_420_SP : RK_FORMAT_YCbCr_420_P);
    int actualDstFormat = (op == Operation::ConvertFormat) ? static_cast<int>(dst_format) : srcFormat;

    int dstWidth = static_cast<int>((Operation::Resize == op) ? dst_width : srcConfig.width);
    int dstHeight = static_cast<int>((Operation::Resize == op) ? dst_height : srcConfig.height);

    rga_buffer_t srcRgaBuf{};           // librga 输入缓冲区结构体
    rga_buffer_t dstRgaBuf{};           // librga 输出缓冲区结构体
    rga_buffer_handle_t srcHandle = 0;  // librga 输入缓冲区句柄（如果使用 fd 模式则为 fd 导出的 handle，否则为 0）
    rga_buffer_handle_t dstHandle = 0;  // 通过检查输入输出缓冲区的 fd 字段来决定是否使用 fd 模式

    bool useFdMode = (src->fd >= 0 && dst->fd >= 0);
    bool forceVirtualMode = false; // 可以通过环境变量或其他配置来强制使用虚拟地址模式进行调试

    LOG_INFO("RGA", "src: fd=", src->fd, ", base=", src->base, ", size=", src->size);
    LOG_INFO("RGA", "dst: fd=", dst->fd, ", base=", dst->base, ", size=", dst->size);
    LOG_INFO("RGA", "useFdMode=", useFdMode);

    if (useFdMode && forceVirtualMode)
    {
        LOG_INFO("RGA", "Trying virtual address mode first (fd mode has issues)");
        useFdMode = false;
    }

    if (useFdMode)
    {
        // 两边都用 fd + handle
        // 注意：importbuffer_fd 需要使用实际的 buffer 分配大小，不是 payload 大小
        size_t srcAllocSize = src->allocSize > 0 ? src->allocSize : src->size;
        srcHandle = importbuffer_fd(src->fd, static_cast<int>(srcAllocSize));
        LOG_INFO("RGA", "importbuffer_fd src: fd=", src->fd, ", allocSize=", srcAllocSize, ", handle=", srcHandle);
        if (srcHandle == 0)
        {
            LOG_ERROR("RGA", "Failed to import source buffer fd: ", src->fd);
            QueueOutputToRecycler(dst);
            return nullptr;
        }
        srcRgaBuf = wrapbuffer_handle(srcHandle,
                                       static_cast<int>(srcConfig.width),
                                       static_cast<int>(srcConfig.height),
                                       srcFormat,
                                       static_cast<int>(srcConfig.horStride),
                                       static_cast<int>(srcConfig.verStride));
        LOG_INFO("RGA", "srcRgaBuf: w=", srcRgaBuf.width, ", h=", srcRgaBuf.height, 
                 ", wstride=", srcRgaBuf.wstride, ", hstride=", srcRgaBuf.hstride);

        size_t dstAllocSize = dst->allocSize > 0 ? dst->allocSize : dst->size;
        dstHandle = importbuffer_fd(dst->fd, static_cast<int>(dstAllocSize));
        LOG_INFO("RGA", "importbuffer_fd dst: fd=", dst->fd, ", allocSize=", dstAllocSize, ", handle=", dstHandle);
        if (dstHandle == 0)
        {
            LOG_ERROR("RGA", "Failed to import destination buffer fd: ", dst->fd);
            releasebuffer_handle(srcHandle);
            QueueOutputToRecycler(dst);
            return nullptr;
        }
        dstRgaBuf = wrapbuffer_handle(dstHandle,
                                       dstWidth,
                                       dstHeight,
                                       actualDstFormat,
                                       static_cast<int>(dst->horStride),
                                       static_cast<int>(dst->verStride));
        LOG_INFO("RGA", "dstRgaBuf: w=", dstRgaBuf.width, ", h=", dstRgaBuf.height,
                 ", wstride=", dstRgaBuf.wstride, ", hstride=", dstRgaBuf.hstride);
    }
    else // 两边都用虚拟地址
    {
        if (!src->base || src->base == MAP_FAILED || src->size == 0)
        {
            LOG_ERROR("RGA", "Source buffer has no valid virtual address");
            QueueOutputToRecycler(dst);
            return nullptr;
        }
        if (!dst->base || dst->base == MAP_FAILED || dst->size == 0)
        {
            LOG_ERROR("RGA", "Destination buffer has no valid virtual address");
            QueueOutputToRecycler(dst);
            return nullptr;
        }

        // 当前虚拟地址路径仅支持 Copy，避免对 Flip/Resize/Convert 产生静默语义降级。
        if (op != Operation::Copy)
        {
            LOG_ERROR("RGA", "Virtual-address path only supports Copy; operation=", static_cast<int>(op));
            QueueOutputToRecycler(dst);
            return nullptr;
        }

        size_t copySize = std::min(src->size, dst->size);
        LOG_INFO("RGA", "Fallback memcpy(copy): src=", src->base, ", dst=", dst->base, ", size=", copySize);
        memcpy(dst->base, src->base, copySize);

        dst->width = srcConfig.width;
        dst->height = srcConfig.height;
        dst->horStride = srcConfig.horStride;
        dst->verStride = srcConfig.verStride;
        dst->format = srcConfig.format;
        dst->dtsUs = src->dtsUs;
        dst->ptsUs = src->ptsUs;

        return dst;
    }

    int ret = imcheck(srcRgaBuf, dstRgaBuf, {}, {});
    LOG_INFO("RGA", "imcheck result: ", ret, " (0=SUCCESS, 2=INVALID_PARAM)");
    
    // 3. 如果 fd 模式下 imcheck 失败，可能是因为某些 fd 导出的 handle 在 RGA 中存在兼容性问题。此时我们可以选择回退到使用虚拟地址的 memcpy 来完成复制操作，虽然性能可能会下降，但至少能保证功能的正确性。
    if (useFdMode && ret != IM_STATUS_SUCCESS)
    {
        if (op == Operation::Copy && src->base && src->base != MAP_FAILED && dst->base && dst->base != MAP_FAILED)
        {
            size_t copySize = std::min(src->size, dst->size);
            LOG_INFO("RGA", "imcheck failed in fd mode, falling back to memcpy(copy), size=", copySize);
            memcpy(dst->base, src->base, copySize);
            dst->width = srcConfig.width;
            dst->height = srcConfig.height;
            dst->horStride = srcConfig.horStride;
            dst->verStride = srcConfig.verStride;
            dst->format = srcConfig.format;
            dst->dtsUs = src->dtsUs;
            dst->ptsUs = src->ptsUs;
            if (srcHandle) releasebuffer_handle(srcHandle);
            if (dstHandle) releasebuffer_handle(dstHandle);
            return dst;
        }

        LOG_ERROR("RGA", "imcheck failed and fallback is not allowed for operation ", static_cast<int>(op));
        if (srcHandle) releasebuffer_handle(srcHandle);
        if (dstHandle) releasebuffer_handle(dstHandle);
        QueueOutputToRecycler(dst);
        return nullptr;
    }
    
    if (ret != IM_STATUS_SUCCESS)
    {
        LOG_ERROR("RGA", "RGA check failed for operation ", static_cast<int>(op), ": ", imStrError(ret));
        LOG_ERROR("RGA", "src: w=", srcRgaBuf.width, ", h=", srcRgaBuf.height, 
                  ", wstride=", srcRgaBuf.wstride, ", hstride=", srcRgaBuf.hstride,
                  ", format=", srcRgaBuf.format);
        LOG_ERROR("RGA", "dst: w=", dstRgaBuf.width, ", h=", dstRgaBuf.height,
                  ", wstride=", dstRgaBuf.wstride, ", hstride=", dstRgaBuf.hstride,
                  ", format=", dstRgaBuf.format);
        if (srcHandle) releasebuffer_handle(srcHandle);
        if (dstHandle) releasebuffer_handle(dstHandle);
        QueueOutputToRecycler(dst);
        return nullptr;
    }

    switch (op)
    {
    case Operation::Copy:
        LOG_INFO("RGA", "Calling imcopy...");
        ret = imcopy(srcRgaBuf, dstRgaBuf);
        LOG_INFO("RGA", "imcopy result: ", ret);
        break;
    case Operation::FlipHorizontal:
        ret = imflip(srcRgaBuf, dstRgaBuf, IM_HAL_TRANSFORM_FLIP_H);
        break;
    case Operation::FlipVertical:
        ret = imflip(srcRgaBuf, dstRgaBuf, IM_HAL_TRANSFORM_FLIP_V);
        break;    
    case Operation::Resize:
        ret = imresize(srcRgaBuf, dstRgaBuf, 
                       static_cast<double>(dst_width) / srcConfig.width, 
                       static_cast<double>(dst_height) / srcConfig.height);
        break;
    case Operation::ConvertFormat:
        ret = imcvtcolor(srcRgaBuf, dstRgaBuf, srcRgaBuf.format, dstRgaBuf.format);
        break;    
    default:
        ret = IM_STATUS_FAILED;
        break;
    }

    if (srcHandle) releasebuffer_handle(srcHandle);
    if (dstHandle) releasebuffer_handle(dstHandle);

    if (ret != IM_STATUS_SUCCESS)
    {
        LOG_ERROR("RGA", "RGA operation ", static_cast<int>(op), " failed: ", imStrError(ret));
        QueueOutputToRecycler(dst);
        return nullptr;
    }

    dst->width = static_cast<uint32_t>(dstWidth);
    dst->height = static_cast<uint32_t>(dstHeight);
    dst->format = static_cast<uint32_t>(actualDstFormat);
    dst->dtsUs = src->dtsUs;
    dst->ptsUs = src->ptsUs;
    return dst;
}

/**
 * @brief 计算 NV12 图像所需的内存大小
 * @param hor_stride 图像水平步幅
 * @param ver_stride 图像垂直步幅
 * @return 计算得到的 NV12 图像内存大小（字节）
 */
size_t RgaProcessor::CalcNv12ImageSize(uint32_t hor_stride, uint32_t ver_stride) const
{
    return static_cast<size_t>(hor_stride * ver_stride * 3 / 2);
}

RgaProcessor::~RgaProcessor()
{
    ReleaseOutputBuffers();
    LOG_INFO("RGA", "RgaProcessor destroyed and resources released");
}

/**
 * @brief 初始化 RGA 处理器
 * @return 0 成功，-1 失败
 */
int RgaProcessor::Init()
{
    if (c_RkRgaInit() != 0)
    {
        LOG_ERROR("RGA", "Failed to initialize RGA library");
        return -1;
    }

    initialized = true;
    LOG_INFO("RGA", "RGA library initialized successfully");

    return 0;
}

/**
 * @brief 执行图像复制操作
 * @param src 输入缓冲区信息结构体指针
 * @return 指向包含复制结果的输出缓冲区信息结构体指针，如果操作失败则返回 nullptr
 */
IoFd *RgaProcessor::Copy(const IoFd *src)
{
    return TransformInternal(src, Operation::Copy);
}

/**
 * @brief 执行图像水平翻转操作
 * @param src 输入缓冲区信息结构体指针
 * @return 指向包含翻转结果的输出缓冲区信息结构体指针，如果操作失败则返回 nullptr
 */
IoFd *RgaProcessor::FlipHorizontal(const IoFd *src)
{
    return TransformInternal(src, Operation::FlipHorizontal);
}

/**
 * @brief 执行图像垂直翻转操作
 * @param src 输入缓冲区信息结构体指针
 * @return 指向包含翻转结果的输出缓冲区信息结构体指针，如果操作失败则返回 nullptr
 */
IoFd *RgaProcessor::FlipVertical(const IoFd *src)
{
    return TransformInternal(src, Operation::FlipVertical);
}

/**
 * @brief 执行图像缩放操作
 * @param src 输入缓冲区信息结构体指针
 * @param dst_width 目标图像宽度
 * @param dst_height 目标图像高度
 * @return 指向包含缩放结果的输出缓冲区信息结构体指针，如果操作失败则返回 nullptr
 */
IoFd *RgaProcessor::Resize(const IoFd *src, uint32_t dst_width, uint32_t dst_height)
{
    return TransformInternal(src, Operation::Resize, dst_width, dst_height);
}

/**
 * @brief 执行图像格式转换操作
 * @param src 输入缓冲区信息结构体指针
 * @param dst_format 目标图像格式
 * @return 指向包含格式转换结果的输出缓冲区信息结构体指针，如果操作失败则返回 nullptr
 */
IoFd *RgaProcessor::ConvertFormat(const IoFd *src, uint32_t dst_format)
{
    return TransformInternal(src, Operation::ConvertFormat, 0, 0, dst_format);
}

/**
 * @brief 将处理完成的输出缓冲区重新加入可用队列以供后续使用
 * @param buffer 指向需要回收的输出缓冲区信息结构体指针
 * @return 0 成功，-1 失败
 */
int RgaProcessor::QueueOutputToRecycler(const IoFd *buffer)
{
    if (!buffer || !IsOutputMember(buffer))
    {
        LOG_ERROR("RGA", "Invalid buffer pointer passed to QueueOutputToRecycler");
        return -1;
    }

    std::lock_guard<std::mutex> lock(outMtx);
    ptrdiff_t index = buffer - outBuffers;
    if (index < 0 || index >= static_cast<ptrdiff_t>(ResourceLimit::RgaOutputBufferCount))
    {
        LOG_ERROR("RGA", "Buffer pointer ", buffer, " is out of range for output buffers in QueueOutputToRecycler");
        return -1;
    }
    outBufferInUse[index] = false;
    availablePositions.push(const_cast<IoFd *>(buffer));
    LOG_INFO("RGA", "Buffer at index ", index, " queued back to recycler");
    return 0;
}

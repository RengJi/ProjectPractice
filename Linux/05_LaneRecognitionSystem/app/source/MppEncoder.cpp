#include <cstring>
#include "MppEncoder.h"
#include "Logger.hpp"

MppEncoder::MppEncoder() = default;

MppEncoder::~MppEncoder()
{
    if (mppCtx)
    {
        mpp_destroy(mppCtx);
        mppCtx = nullptr;
        mppApi = nullptr;
    }

    LOG_INFO("MPP_ENC", "MPP encoder resources released");
}

/**
 * @brief 初始化MPP编码器
 * @return int 初始化结果，0表示成功，-1表示失败
 */
int MppEncoder::Init()
{
    if (mpp_create(&mppCtx, &mppApi) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to create MPP context");
        return -1;
    }

    if (mpp_init(mppCtx, MPP_CTX_ENC, MPP_ENC_DEFAULT_CODINGTYEP) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to initialize MPP encoder");
        mpp_destroy(mppCtx);
        mppCtx = nullptr;
        mppApi = nullptr;
        return -1;
    }

    LOG_INFO("MPP_ENC", "MPP encoder initialized successfully");
    return 0;
}

/**
 * @brief 设置编码器的宽高和步幅
 * @param width 视频宽度
 * @param height 视频高度
 * @param hor_stride 水平步幅，必须大于等于width，如果为0则自动计算为(width + 15) & ~15
 * @param ver_stride 垂直步幅，必须大于等于height，如果为0则自动计算为(height + 15) & ~15
 * @return int 设置结果，0表示成功，-1表示失败
 */
int MppEncoder::SetWidthHeight(uint32_t width, uint32_t height, uint32_t hor_stride, uint32_t ver_stride)
{
    // 1. 设置预处理配置
    this->width = width;
    this->height = height;
    this->horStride = hor_stride > 0 ? hor_stride : ((width + 15) & ~15);
    this->verStride = ver_stride > 0 ? ver_stride : ((height + 15) & ~15);

    memset(&prepCfg, 0, sizeof(prepCfg));
    prepCfg.change = MPP_ENC_PREP_CFG_CHANGE_INPUT | MPP_ENC_PREP_CFG_CHANGE_ROTATION | MPP_ENC_PREP_CFG_CHANGE_FORMAT; // 申明需要更新的配置项
    prepCfg.width = this->width;
    prepCfg.height = this->height;
    prepCfg.hor_stride = this->horStride;
    prepCfg.ver_stride = this->verStride;
    prepCfg.format = MPP_FMT_YUV420SP;          // 输入格式为NV12
    prepCfg.rotation = MPP_ENC_ROT_0;           // 不旋转

    if (mppApi->control(mppCtx, MPP_ENC_SET_PREP_CFG, &prepCfg) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to set encoder prep configuration");
        return -1;
    }

    // 2. 设置码率控制配置
    memset(&rcCfg, 0, sizeof(rcCfg));
    rcCfg.change = MPP_ENC_RC_CFG_CHANGE_ALL;   // 申明需要更新的配置项
    rcCfg.rc_mode = MPP_ENC_RC_MODE_CBR;        // 固定码率控制模式
    rcCfg.quality = MPP_ENC_RC_QUALITY_MEDIUM;  // 中等质量
    rcCfg.bps_target = bitRate;                 // 目标码率
    rcCfg.bps_min = bitRate / 2;                // 最小码率
    rcCfg.bps_max = bitRate * 2;                // 最大码率
    rcCfg.gop = gop;                            // 关键帧间隔
    rcCfg.fps_in_flex = 0;                      // 输入帧率不变
    rcCfg.fps_in_num = fps;                     // 输入帧率
    rcCfg.fps_in_denorm = 1;                    // 输入帧率分母
    rcCfg.fps_out_flex = 0;                     // 输出帧率不变 
    rcCfg.fps_out_num = fps;                    // 输出帧率
    rcCfg.fps_out_denorm = 1;                   // 输出帧率分母

    if (mppApi->control(mppCtx, MPP_ENC_SET_RC_CFG, &rcCfg) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to set encoder rate control configuration");
        return -1;
    }

    // 3. 设置编码器配置
    memset(&codecCfg, 0, sizeof(codecCfg));
    codecCfg.coding = MPP_ENC_DEFAULT_CODINGTYEP;                                                                             
    codecCfg.change = MPP_ENC_H264_CFG_CHANGE_PROFILE | MPP_ENC_H264_CFG_CHANGE_ENTROPY | MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;    // 申明需要更新的配置项
    codecCfg.h264.profile = 100;            // 高质量                                                        
    codecCfg.h264.level = 41;               // 1080p@30fps
    codecCfg.h264.entropy_coding_mode = 0;  // 0表示CABAC，1表示CAVLC
    codecCfg.h264.cabac_init_idc = 0;       // CABAC初始化IDC，0表示默认初始化，1-2表示特定初始化
    codecCfg.h264.transform8x8_mode = 1;    // 0表示不使用8x8变换，1表示使用8x8变换，提高压缩比
    if (mppApi->control(mppCtx, MPP_ENC_SET_CODEC_CFG, &codecCfg) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to set encoder codec configuration");
        return -1;
    }

    LOG_INFO("MPP_ENC", "Encoder configuration set successfully: width=%u, height=%u, hor_stride=%u, ver_stride=%u, fps=%u, bitrate=%u, gop=%u",
             this->width, this->height, this->horStride, this->verStride, fps, bitRate, gop);
    return 0;
}

/**
 * @brief 输入一帧视频数据进行编码
 * @param io 输入帧的文件描述符和内存映射信息，必须包含有效的fd、base和size
 * @return int 输入结果，0表示成功，-1表示失败
 */
int MppEncoder::PutFrame(const IoFd *io)
{
    if (!io || io->fd < 0 || !io->base || io->size == 0)
    {
        LOG_ERROR("MPP_ENC", "Invalid input frame parameters");
        return -1;
    }

    std::lock_guard<std::mutex> lock(encMutex); // 确保线程安全

    // 1. 创建MPP帧并设置属性
    MppFrame frame = nullptr;
    if (mpp_frame_init(&frame) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to initialize MPP frame");
        return -1;
    }

    mpp_frame_set_width(frame, width);
    mpp_frame_set_height(frame, height);
    mpp_frame_set_hor_stride(frame, horStride);
    mpp_frame_set_ver_stride(frame, verStride);
    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP); // 输入格式为NV12
    mpp_frame_set_pts(frame, io->ptsUs);
    mpp_frame_set_dts(frame, io->dtsUs);

    // 2. 导入输入帧的缓冲区到MPP
    MppBufferInfo_t bufferInfo{};
    bufferInfo.type = MPP_BUFFER_TYPE_ION;
    bufferInfo.size = io->size;
    bufferInfo.ptr = io->base;
    bufferInfo.fd = io->fd;

    MppBuffer buffer = nullptr;
    if (mpp_buffer_import(&buffer, &bufferInfo) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to import buffer for frame");
        mpp_frame_deinit(&frame);
        return -1;
    }
    
    
    mpp_frame_set_buffer(frame, buffer);
    mpp_buffer_put(buffer);                 // 释放对buffer的引用，编码器内部会增加引用计数

    if (mppApi->poll(mppCtx, MPP_PORT_INPUT, MPP_POLL_BLOCK) != MPP_OK) // 阻塞等待输入端口可用
    {
        LOG_ERROR("MPP_ENC", "Failed to poll encoder input port");
        mpp_frame_deinit(&frame);
        return -1;
    }

    // 3. 将帧发送到编码器输入端口
    MppTask task = nullptr;
    if (mppApi->dequeue(mppCtx, MPP_PORT_INPUT, &task) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to dequeue task from encoder input port");
        mpp_frame_deinit(&frame);
        return -1;
    }

    mpp_task_meta_set_frame(task, KEY_INPUT_FRAME, frame);

    if (mppApi->enqueue(mppCtx, MPP_PORT_INPUT, task) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to enqueue task to encoder input port");
        mpp_frame_deinit(&frame);
        return -1;
    }

    mpp_frame_deinit(&frame); // 释放对frame的引用，编码器内部会增加引用计数
    LOG_INFO("MPP_ENC", "Frame enqueued successfully: width=%u, height=%u, hor_stride=%u, ver_stride=%u, pts=%lld, dts=%lld",
             width, height, horStride, verStride, io->ptsUs, io->dtsUs);
    return 0;
}

/**
 * @brief 从编码器输出端口获取编码数据包
 * @param result 编码数据包结果结构体，成功时包含有效的数据指针、大小和时间戳等信息
 * @return int 获取结果，0表示成功获取数据包，1表示没有数据包可用，-1表示发生错误
 */
int MppEncoder::GetPacket(MppEncPacketResult &result)
{
    std::lock_guard<std::mutex> lock(encMutex); // 确保线程安全

    // 1. 轮询检查编码器输出端口是否有数据包可用
    if (mppApi->poll(mppCtx, MPP_PORT_OUTPUT, MPP_POLL_NON_BLOCK) != MPP_OK)
    {
        return MppEncPacketStatus::NoPacket; // 没有数据包可用
    }

    // 2. 从输出端口获取数据包
    MppTask task = nullptr;
    if (mppApi->dequeue(mppCtx, MPP_PORT_OUTPUT, &task) != MPP_OK)
    {
        LOG_ERROR("MPP_ENC", "Failed to dequeue task from encoder output port");
        return MppEncPacketStatus::Error;
    }

    // 3. 从任务元数据中获取编码数据包
    MppPacket packet;
    MPP_RET ret = mpp_task_meta_get_packet(task, KEY_OUTPUT_PACKET, &packet);
    if (ret != MPP_OK || !packet)
    {
        LOG_ERROR("MPP_ENC", "No packet found in task metadata");
        mppApi->enqueue(mppCtx, MPP_PORT_OUTPUT, task); // 重新入队
        return MppEncPacketStatus::Error;
    }

    // 4. 提取数据包信息并填充结果结构体
    int size = mpp_packet_get_length(packet);
    const void *data = mpp_packet_get_data(packet);

    result.data = data;
    result.size = size;
    result.dtsUs = mpp_packet_get_dts(packet);
    result.ptsUs = mpp_packet_get_pts(packet);
    result.eos = mpp_packet_get_eos(packet);
    result.eoi = true;
    result.handle = (void *)packet;
    
    packetEos = result.eos; // 更新数据包结束标志

    mppApi->enqueue(mppCtx, MPP_PORT_OUTPUT, task);
    LOG_INFO("MPP_ENC", "Packet dequeued successfully: size=", size, ", pts=", result.ptsUs, ", dts=", result.dtsUs, ", eos=", result.eos);
    return MppEncPacketStatus::HasPacket;
}

/**
 * @brief 释放编码数据包资源
 * @param result 编码数据包结果结构体，必须包含有效的handle
 * @return int 释放结果，0表示成功，-1表示失败
 */
int  MppEncoder::ReleasePacket(MppEncPacketResult &result)
{
    if (!result.handle)
    {
        LOG_ERROR("MPP_ENC", "Invalid packet handle for release");
        return -1;
    }

    MppPacket packet = static_cast<MppPacket>(result.handle);
    mpp_packet_deinit(&packet);
    LOG_INFO("MPP_ENC", "Packet released successfully");
    return 0;
}
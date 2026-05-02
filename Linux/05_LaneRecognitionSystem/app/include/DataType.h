#pragma once 

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// 规定一些数量限制
namespace ResourceLimit
{
    inline constexpr size_t MaxAllocSize = 48U;                     // 最大数量限制
    inline constexpr size_t CameraRequestBufferCount = 16U;         // 摄像头请求缓冲区数量限制
    inline constexpr size_t CameraMappedBufferCount = MaxAllocSize; // 摄像头映射缓冲区数量限制
    inline constexpr size_t MppImportBufferCount = MaxAllocSize;    // MPP导入缓冲区数量限制
    inline constexpr size_t MppOutputBufferCount = MaxAllocSize;    // MPP输出缓冲区数量限制
    inline constexpr size_t RgaOutputBufferCount = MaxAllocSize;    // RGA输出缓冲区数量限制
    inline constexpr size_t InferenceBufferCount = 8U;              // 推理缓冲区数量限制
}


// 摄像头的原始帧描述结构体
struct FrameDescription
{
    int index = -1;                 // 帧索引
    int fd = -1;                    // 文件描述符
    void *base = nullptr;           // 映射的内存地址
    size_t length = 0;              // 映射的内存长度
    size_t payloadSize = 0;         // 有效载荷大小
    int64_t dtsUs = -1;             // 解码时间戳（微秒）
    int64_t ptsUs = -1;             // 显示时间戳（微秒）
    bool cpuSyncReadActive = false; // CPU同步读取标志
};

// 编解码输出文件描述符结构体
struct IoFd 
{
    int fd = -1;            // 文件描述符
    void *base = nullptr;   // 映射的内存地址
    size_t size = 0;        // 有效数据大小
    size_t allocSize = 0;   // 实际分配大小（用于 importbuffer_fd）
    uint32_t format = 0;    // 像素格式
    uint32_t width = 0;     // 图像宽度
    uint32_t height = 0;    // 图像高度
    uint32_t horStride = 0; // 水平步幅
    uint32_t verStride = 0; // 垂直步幅
    int64_t dtsUs = -1;     // 解码时间戳（微秒）
    int64_t ptsUs = -1;     // 显示时间戳（微秒）
    void *cameraBuffer = nullptr; // V4L2 相机缓冲区指针 (FrameDescription*)，用于延迟回收
};

// 视频捕获配置结构体
struct VedioCaptureConfig 
{
    uint32_t width = 0;         // 视频宽度
    uint32_t height = 0;        // 视频高度
    uint32_t pixelFormat = 0;   // 视频像素格式
    uint32_t bytesPerLine = 0;  // 每行字节数
    uint32_t imageSize = 0;     // 视频图像大小
    uint32_t fpsNum = 0;        // 视频帧率分子
    uint32_t fpsDen = 0;        // 视频帧率分母
    bool valid = false;         // 配置是否有效
};


// 车道线检测结果结构体
struct LaneDetectionResult
{
    struct LanePoint
    {
        int x; // 车道线点的x坐标
        int y; // 车道线点的y坐标
    };
    std::vector<std::vector<LanePoint>> lanes;  // 每条车道线的点集合
    std::vector<int> laneTypes;                 // 每条车道线的类型（例如：实线、虚线等）
    int64_t timeStampUs = -1;                   // 结果的时间戳（微秒）
    bool valid = false;                         // 结果是否有效
};

// 管道状态结构体
struct PipelineStatus
{
    uint64_t capturedFrames = 0;   // 已捕获的帧数
    uint64_t processedFrames = 0;  // 已处理的帧数
    uint64_t droppedFrames = 0;    // 已丢弃的帧
    uint64_t decodedFrames = 0;    // 已解码的帧数
    uint64_t inferenceFrames = 0;  // 已推理的帧数
    uint64_t displayedFrames = 0;  // 已显示的帧数
    double captureFps = 0.0;       // 捕获帧率
    double processingFps = 0.0;    // 处理帧率
    double inferenceFps = 0.0;     // 推理帧率
    double e2eLatencyMs = 0.0;     // 端到端延迟（毫秒）
};

// 定义一个枚举类来表示管道的状态
enum class PipelineState
{
    Idle,       // 空闲
    Running,    // 运行
    Paused,     // 暂停
    Stopped,    // 停止
    Error       // 错误
};
#pragma once

#include <linux/videodev2.h>
#include <vector>
#include <string.h>
#include "DataType.h"

// 摄像头参数配置
namespace CameraParameter
{
    inline constexpr __u32 CaptureWidth = 1920;
    inline constexpr __u32 CaptureHeight = 1080;
    inline constexpr __u32 CaptureFps = 30;
    inline constexpr size_t MaxFrameSize = 4096 * 4096;
    inline constexpr __u32 CaptureRequestBufferCount = static_cast<__u32>(ResourceLimit::CameraRequestBufferCount); // 请求缓冲区数量
    inline constexpr __u32 CaptureMappedBufferCount = static_cast<__u32>(ResourceLimit::CameraMappedBufferCount);   // 映射缓冲区数量
    inline constexpr int DequeueTimeoutMs = 5000;   // V4L2缓冲区出队超时时间（毫秒）
}

// 摄像头读取结果状态码
namespace CameraReadResult
{
    inline constexpr int ok = 0;        // 成功
    inline constexpr int retryAble = 1; // 读取失败但可以重试（例如缓冲区暂时不可用）
    inline constexpr int timeOut = 2;   // 读取超时
    inline constexpr int fatal = -1;    // 读取失败且不可重试（例如设备错误或缓冲区损坏）
}

class V4L2Camera
{
public:
    V4L2Camera();
    ~V4L2Camera();

    int Init(const std::string &devicePath, int expousetime_ms = 0);
    int Open(const std::string &devicePath);
    int SetCameraFormat();
    int InitBuffer();
    int DeinitBuffer();
    int StreamControl(bool enable);
    int ReadFrame(int *outlength, size_t maxlength = CameraParameter::MaxFrameSize);
    int DequeueFrame(unsigned int &index, unsigned int &bufer_size, size_t max_buffer_size);
    int RequeueFrame(FrameDescription *frame_des);
    int SetExposureTime(int exposureTimeMs);

    FrameDescription *GetCurrentFrameDescription() const { return curFrameDes; }
    int GetBufferCount() const { return bufferCount; }
    VedioCaptureConfig &GetCaptureConfig() { return captureConfig; }
    bool IsMultiplanar() const { return isMultiplanar; }

private:
    int Close();

    int cameraFd = -1;                          // 摄像头设备文件描述符
    struct v4l2_capability cameraCapability{};  // 摄像头设备能力结构体
    struct v4l2_format cameraFormat{};          // 摄像头格式结构体
    struct v4l2_streamparm cameraStreamParam{}; // 摄像头流参数结构体
    bool isStreaming = false;                   // 摄像头是否正在流式传输
    bool isMultiplanar = false;                 // 是否为多平面模式 (MPLANE)
    int bufferCount = 0;                        // 请求的缓冲区数量
    VedioCaptureConfig captureConfig{};         // 存储摄像头实际生效的配置
    FrameDescription *curFrameDes = nullptr;    // 存储抓取到的帧描述信息的指针
public:
    FrameDescription frameDesArray[CameraParameter::CaptureMappedBufferCount]; // 帧描述数组
};
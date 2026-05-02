#pragma once

#include <string>
#include <cstdint>

namespace Config
{
    namespace Camera 
    {
        inline constexpr const char *CameraPath = "/dev/video11";   // 摄像头设备路径
        inline constexpr uint32_t CameraWidth = 1920;               // 摄像头分辨率宽度
        inline constexpr uint32_t CameraHeight = 1080;              // 摄像头分辨率高度
        inline constexpr uint32_t CameraFPS = 30;                   // 摄像头帧率
        inline constexpr uint32_t CameraDequeueTimeoutMs = 1000;    // 摄像头队列超时时间（毫秒）
    }

    namespace Codec
    {
        inline constexpr uint32_t EncodeBitRate = 2000000;      // 编码比特率（bps）
        inline constexpr uint32_t EncoderGop = 60;              // 编码GOP大小（帧数）
        inline constexpr const char *EncoderFormat = "h264";    // 编码格式
    }

    namespace PreProcessing
    {
        inline constexpr bool EnableLIME = false;   // 是否启用LIME算法进行图像增强
        inline constexpr float LIMEAlpha = 1.0f;    // LIME算法的增强强度参数
        inline constexpr float LIMEBeta = 2.0f;     // LIME算法的平滑参数
        inline constexpr float LIMEGama = 0.7f;     // LIME算法的伽马校正参数
        inline constexpr int LIMEInputWidth = 320;  // LIME处理分辨率宽度
        inline constexpr int LIMEInputHeight = 180; // LIME处理分辨率高度
        inline constexpr int LIMEFrameInterval = 2; // 每N帧执行一次LIME，1表示每帧执行
    }

    namespace Inference
    {
        inline constexpr uint32_t ModelInputWidth = 224;        // 模型输入图像宽度
        inline constexpr uint32_t ModelInputHeight = 224;       // 模型输入图像高度
        inline constexpr const char *ModelPath = "/root/03_project/06_LaneRecognitionSystem/model/pp_liteseg.rknn"; // 模型文件路径
        inline constexpr float ConfidenceThreshold = 0.5f;      // 推理结果置信度阈值
        inline constexpr uint32_t TargetInputFps = 10;          // 推理入口目标帧率，0 表示不节流
    }

    namespace Display
    {
        inline constexpr bool EnableDisplayOverlay = true;    // 是否启用显示叠加
        inline constexpr uint32_t DisplayWidth = 1280;        // 显示窗口宽度
        inline constexpr uint32_t DisplayHeight = 720;        // 显示窗口高度
    }

    namespace Pipeline
    {
        inline constexpr size_t MaxQueueDepth = 10;         // 各处理阶段的最大队列深度
        inline constexpr uint32_t StatIntervalMs = 1000;    // 统计信息输出间隔（毫秒）
        inline constexpr bool DisplayLatestOnly = true;     // 显示队列仅保留最新帧，降低延迟和卡顿
    }
}

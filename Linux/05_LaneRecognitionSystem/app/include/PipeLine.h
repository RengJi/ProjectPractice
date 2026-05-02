#pragma once

#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <functional>
#include "V4L2Camera.h"
#include "MppDecoder.h"
#include "MppEncoder.h"
#include "RgaProcess.h"
#include "LaneDetector.h"
#include "DataType.h"
#include "LimeEnhance.h"

// 视频处理管道类，负责管理整个视频处理流程，包括捕获、解码、处理、编码和显示
class PipeLine
{
private:
    void CaptureLoop();   // 捕获线程函数
    void ProcessLoop();   // 处理线程函数
    void DecodeLoop();    // 编码线程函数
    void InferenceLoop(); // 推理线程函数
    void DisplayLoop();   // 显示线程函数

    void RequestStop();
    void NotifyAllThreads();

public:
    using FrameCallback = std::function<void(const cv::Mat &, const SegmentationResult &)>; // 帧回调函数类型

    PipeLine();
    ~PipeLine();

    int Init(const std::string &device_path, int width, int height, int fps,
             const std::string &model_path, bool enable_lime);
    void Start();
    void Stop();
    void Pause();
    void Resume();

    void SetFrameCallback(FrameCallback callback) { frameCallback = std::move(callback); }
    PipelineState GetState() const { return status.load(); }
    PipelineStatus GetPipelineStatus() const;

    void SetLimeParameters(bool enable, float alpha = 1.0f)
    {
        enableLime = enable;
        limeAlpha = alpha;
    }

private:
    V4L2Camera camera;
    MppDecoder decoder;
    MppEncoder encoder;
    RgaProcessor rgaProcessor;
    LaneDetector laneDetector;

    std::thread captureThread;
    std::thread processThread;
    std::thread decodeThread;
    std::thread inferenceThread;
    std::thread displayThread;

    std::mutex captureQueueMutex;
    std::mutex processQueueMutex;
    std::mutex decodeQueueMutex;
    std::mutex inferenceQueueMutex;
    std::mutex displayQueueMutex;

    std::condition_variable captureQueueCV;
    std::condition_variable processQueueCV;
    std::condition_variable decodeQueueCV;
    std::condition_variable inferenceQueueCV;
    std::condition_variable displayQueueCV;

    std::deque<FrameDescription *> captureQueue;
    std::deque<const IoFd *> processQueue;
    std::deque<const IoFd *> decodeQueue;
    std::deque<cv::Mat> inferenceQueue;
    std::deque<std::pair<cv::Mat, SegmentationResult>> displayQueue;

    std::atomic<bool> stop{false};                          // 申请停止标志
    std::atomic<bool> paused{false};                        // 暂停标志
    std::atomic<PipelineState> status{PipelineState::Idle}; // 当前状态

    std::atomic<uint64_t> captureFrames{0};   // 已捕获的帧数
    std::atomic<uint64_t> processedFrames{0}; // 已处理的帧数
    std::atomic<uint64_t> droppedFrames{0};   // 已丢弃的帧数（节流或队列满）
    std::atomic<uint64_t> decodedFrames{0};   // 已解码的帧数
    std::atomic<uint64_t> inferenceFrames{0}; // 已推理的帧数
    std::atomic<uint64_t> displayedFrames{0}; // 已显示的帧数

    FrameCallback frameCallback; // 帧回调函数
    std::string modelPath;       // 模型路径
    std::string devicePath;      // 设备路径
    int width = 1920;            // 视频宽度
    int height = 1080;           // 视频高度
    int fps = 30;                // 视频帧率

    bool needDecode = true; // 是否需要解码
    bool enableLime = true; // 是否启用LIME算法
    float limeAlpha = 1.0f; // LIME算法增强强度

    std::chrono::steady_clock::time_point startTime; // 管道启动时间
};

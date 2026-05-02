#include "PipeLine.h"
#include "Logger.hpp"
#include "Config.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <sys/mman.h>
#include <utility>

#ifndef MPP_ALAIN
#define MPP_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#endif

PipeLine::PipeLine() = default;

PipeLine::~PipeLine()
{
    Stop();
}

/**
 * @brief 初始化视频处理管道，包括摄像头、解码器、RGA处理器和车道线检测器
 * @param device_path 摄像头设备路径
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频帧率
 * @param model_path 车道线检测模型路径
 * @param enable_lime 是否启用LIME算法进行图像增强
 * @return 初始化结果，0表示成功，非0表示失败
 */
int PipeLine::Init(const std::string &device_path, int width, int height, int fps,
                   const std::string &model_path, bool enable_lime)
{
    cv::ocl::setUseOpenCL(false);
    
    // 1. 初始化基础参数
    devicePath = device_path;
    this->width = width;
    this->height = height;
    this->fps = fps;
    modelPath = model_path;
    enableLime = enable_lime;

    // 2. 初始化摄像头
    int ret = camera.Init(devicePath);
    if (ret != 0)
    {
        LOG_ERROR("PIPELINE", "Failed to initialize camera with device path: ", devicePath);
        return -1;
    }

    const auto &captureConfig = camera.GetCaptureConfig();
    if (!captureConfig.valid)
    {
        LOG_ERROR("PIPELINE", "Camera capture configuration is invalid");
        return -1;
    }

    // 3. 确认是否需要初始化解码器, 需要的话初始化解码器
    needDecode = (captureConfig.pixelFormat == V4L2_PIX_FMT_MJPEG) ||
                 (captureConfig.pixelFormat == V4L2_PIX_FMT_H264) ||
                 (captureConfig.pixelFormat == V4L2_PIX_FMT_HEVC) ||
                 (captureConfig.pixelFormat == V4L2_PIX_FMT_VP8) ||
                 (captureConfig.pixelFormat == V4L2_PIX_FMT_VP9);
    if (needDecode)
    {
        if (decoder.Init() != 0)
        {
            LOG_ERROR("PIPELINE", "Failed to initialize MPP decoder");
            return -1;
        }
        decoder.SetWidthHeight(captureConfig.width, captureConfig.height);
        decoder.AllocInputBuffers(camera.frameDesArray, camera.GetBufferCount());

        char codeFormatStr[5] = {0};
        memcpy(codeFormatStr, &captureConfig.pixelFormat, 4);
        LOG_INFO("PIPELINE", "Camera initialized: ", captureConfig.width, "x", captureConfig.height, 
                 ", format: ", codeFormatStr, ", fps: ", captureConfig.fpsNum, "/", captureConfig.fpsDen);
    }
    else
    {
        char codeFormatStr[5] = {0};
        memcpy(codeFormatStr, &captureConfig.pixelFormat, 4);
        LOG_INFO("PIPELINE", "Camera initialized: ", captureConfig.width, "x", captureConfig.height, 
                 ", format: ", codeFormatStr, ", fps: ", captureConfig.fpsNum, "/", captureConfig.fpsDen);
    }

    // 4. 初始化 RGA 处理器
    if (rgaProcessor.Init() != 0)
    {
        LOG_ERROR("PIPELINE", "Failed to initialize RGA processor");
        return -1;
    }

    // 5. 初始化车道线检测器
    if (laneDetector.Init(modelPath) != 0)
    {
        LOG_ERROR("PIPELINE", "Failed to initialize lane detector with model path: ", modelPath);
        return -1;
    }

    LOG_INFO("PIPELINE", "Pipeline initialized successfully");
    return 0;
}

/**
 * @brief 启动视频处理管道，开启各个线程进行视频捕获、处理、编码、推理和显示
 * @return 无返回值
 */
void PipeLine::Start()
{
    if (status.load() != PipelineState::Idle || status.load() == PipelineState::Paused)
    {
        LOG_WARNING("PIPELINE", "Pipeline is not in idle state, cannot start. Current state: ", static_cast<int>(status.load()));
        return;
    }

    // 1. 更新状态并重置统计数据
    stop.store(false);
    paused.store(false);
    status.store(PipelineState::Running);
    startTime = std::chrono::steady_clock::now();

    // 2. 开启流
    camera.StreamControl(true);

    // 3. 开启各个线程
    captureThread = std::thread(&PipeLine::CaptureLoop, this);
    processThread = std::thread(&PipeLine::ProcessLoop, this);
    decodeThread = std::thread(&PipeLine::DecodeLoop, this);
    inferenceThread = std::thread(&PipeLine::InferenceLoop, this);
    displayThread = std::thread(&PipeLine::DisplayLoop, this);

    LOG_INFO("PIPELINE", "Pipeline started successfully");
}

/**
 * @brief 停止视频处理管道，安全地关闭各个线程并释放资源
 * @return 无返回值
 */
void PipeLine::Stop()
{
    if (status.load() == PipelineState::Idle || status.load() == PipelineState::Stopped)
    {
        LOG_WARNING("PIPELINE", "Pipeline is already stopped or idle, cannot stop. Current state: ", static_cast<int>(status.load()));
        return;
    }

    // 1. 请求所有线程停止
    RequestStop();
    status.store(PipelineState::Stopped);

    // 2. 通知所有线程，避免线程在等待条件变量时无法退出
    NotifyAllThreads();

    // 3. 等待所有线程退出
    if (captureThread.joinable())
        captureThread.join();
    if (processThread.joinable())
        processThread.join();
    if (decodeThread.joinable())
        decodeThread.join();
    if (inferenceThread.joinable())
        inferenceThread.join();
    if (displayThread.joinable())
        displayThread.join();

    // 4. 停止摄像头流
    camera.StreamControl(false);

    // 5. 清除队列中的数据，重置统计数据
    {
        std::lock_guard<std::mutex> lock1(captureQueueMutex);
        std::lock_guard<std::mutex> lock2(processQueueMutex);
        std::lock_guard<std::mutex> lock3(decodeQueueMutex);
        std::lock_guard<std::mutex> lock4(inferenceQueueMutex);
        std::lock_guard<std::mutex> lock5(displayQueueMutex);
        captureQueue.clear();
        processQueue.clear();
        decodeQueue.clear();
        inferenceQueue.clear();
        displayQueue.clear();
    }

    status.store(PipelineState::Idle);
    LOG_INFO("PIPELINE", "Pipeline stopped successfully");
}

/**
 * @brief 暂停视频处理管道，暂停各个线程的工作但不释放资源
 * @return 无返回值
 */
void PipeLine::Pause()
{
    if (status.load() != PipelineState::Running)
    {
        LOG_WARNING("PIPELINE", "Pipeline is not running, cannot pause. Current state: ", static_cast<int>(status.load()));
        return;
    }
    paused.store(true);
    status.store(PipelineState::Paused);
    LOG_INFO("PIPELINE", "Pipeline paused successfully");
}

/**
 * @brief 恢复视频处理管道，恢复各个线程的工作
 * @return 无返回值
 */
void PipeLine::Resume()
{
    if (status.load() != PipelineState::Paused)
    {
        LOG_WARNING("PIPELINE", "Pipeline is not paused, cannot resume. Current state: ", static_cast<int>(status.load()));
        return;
    }
    paused.store(false);
    status.store(PipelineState::Running);
    NotifyAllThreads(); // 恢复时通知所有线程，确保它们能够及时响应恢复请求
    LOG_INFO("PIPELINE", "Pipeline resumed successfully");
}

/**
 * @brief 获取当前管道状态，包括帧数统计和性能指标
 * @return 当前管道状态的结构体，包含帧数统计和性能指标
 */
PipelineStatus PipeLine::GetPipelineStatus() const
{
    PipelineStatus status;
    status.capturedFrames = captureFrames.load();
    status.decodedFrames = decodedFrames.load();
    status.processedFrames = processedFrames.load();
    status.droppedFrames = droppedFrames.load();
    status.inferenceFrames = inferenceFrames.load();
    status.displayedFrames = displayedFrames.load();

    auto now = std::chrono::steady_clock::now();
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();
    status.captureFps = elapsedSeconds > 0 ? captureFrames.load() / elapsedSeconds : 0.0;
    status.processingFps = elapsedSeconds > 0 ? processedFrames.load() / elapsedSeconds : 0.0;
    status.inferenceFps = elapsedSeconds > 0 ? inferenceFrames.load() / elapsedSeconds : 0.0;

    return status;
}

/**
 * @brief 捕获线程函数，从摄像头捕获视频帧并将其放入处理队列
 * @return 无返回值
 */
void PipeLine::CaptureLoop()
{
    while (!stop.load())
    {
        if (paused.load())
        {
            std::unique_lock<std::mutex> lock(captureQueueMutex);
            captureQueueCV.wait(lock, [this]
                                { return !paused.load() || stop.load(); });
            if (stop.load())
                break;
        }

        // 1. 从摄像头读取一帧数据
        int frameSize = 0;
        int ret = camera.ReadFrame(&frameSize);

        if (ret == CameraReadResult::retryAble)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2)); // 短暂等待后重试
            continue;
        }
        else if (ret != CameraReadResult::ok)
        {
            LOG_ERROR("PIPELINE", "Failed to read frame from camera, error code: ", ret);
            continue;
        }

        FrameDescription *frameDes = camera.GetCurrentFrameDescription();
        if (frameDes == nullptr || frameDes->base == nullptr || frameDes->length == 0)
        {
            LOG_ERROR("PIPELINE", "Invalid frame description received from camera");
            continue;
        }

        // 2. 将捕获的帧放入处理队列
        {
            std::lock_guard<std::mutex> lock(captureQueueMutex);
            if (captureQueue.size() > Config::Pipeline::MaxQueueDepth)
            {
                FrameDescription *oldFrameDes = captureQueue.front();
                camera.RequeueFrame(oldFrameDes); // 将旧帧重新入队，释放资源
                captureQueue.pop_front();
                LOG_WARNING("PIPELINE", "Capture queue is full, dropping oldest frame");
            }
            captureQueue.push_back(frameDes);
        }
        decodeQueueCV.notify_one(); // 通知解码线程有新帧可处理
        captureFrames.fetch_add(1);
    }
}

/**
 * @brief 解码线程函数，从解码队列获取视频帧进行解码，解码后将原始图像放入推理队列
 * @return 无返回值
 */
void PipeLine::DecodeLoop()
{
    while (!stop.load())
    {
        if (paused.load())
        {
            std::unique_lock<std::mutex> lock(decodeQueueMutex);
            decodeQueueCV.wait(lock, [this]
                               { return !paused.load() || stop.load(); });
            if (stop.load())
                break;
        }

        // 1. 从解码队列获取一帧数据
        FrameDescription *frameDes = nullptr;
        {
            std::unique_lock<std::mutex> lock(captureQueueMutex);
            decodeQueueCV.wait(lock, [this]
                               { return !captureQueue.empty() || stop.load(); });
            if (!captureQueue.empty())
            {
                frameDes = captureQueue.front();
                captureQueue.pop_front();
            }
            else
            {
                if (stop.load())
                    break;
                else
                    continue;
            }
        }

        if (frameDes == nullptr)
        {
            LOG_ERROR("PIPELINE", "Failed to get frame from capture queue for decoding");
            continue;
        }

        // 2. 解码帧数据
        if (needDecode)
        {
            if (decoder.Decode(frameDes) == 0)
            {
                const IoFd *outputDesc = decoder.currentOutputDesc;
                if (outputDesc != nullptr)
                {
                    {
                        std::lock_guard<std::mutex> lock(processQueueMutex);
                        processQueue.push_back(outputDesc);
                    }
                    processQueueCV.notify_one();
                    decodedFrames.fetch_add(1);
                }
                else
                {
                    LOG_ERROR("PIPELINE", "Decoder did not provide an output buffer after decoding");
                }
            }
        }
        else
        {
            const auto &captureConfig = camera.GetCaptureConfig();
            uint32_t stride = captureConfig.bytesPerLine > 0 ? captureConfig.bytesPerLine : static_cast<uint32_t>(this->width);
            uint32_t frameHeight = static_cast<uint32_t>(this->height);
            
            size_t yPlaneSize = stride * frameHeight;
            size_t uvPlaneSize = stride * frameHeight / 2;
            size_t nv12Size = yPlaneSize + uvPlaneSize;
            
            size_t dataSize = frameDes->payloadSize;
            if (dataSize < nv12Size)
            {
                LOG_WARNING("PIPELINE", "Frame payload size (", dataSize, 
                           ") is smaller than expected NV12 size (", nv12Size, "), using actual size");
                nv12Size = dataSize;
            }
            
            IoFd *outputDesc = new IoFd{
                .fd = -1,
                .base = frameDes->base,
                .size = nv12Size,
                .allocSize = 0,
                .format = MPP_FMT_YUV420SP,
                .width = static_cast<uint32_t>(this->width),
                .height = static_cast<uint32_t>(this->height),
                .horStride = stride,
                .verStride = frameHeight,
                .dtsUs = frameDes->dtsUs,
                .ptsUs = frameDes->ptsUs,
                .cameraBuffer = frameDes};
            {
                std::lock_guard<std::mutex> lock(processQueueMutex);
                processQueue.push_back(outputDesc);
            }
            processQueueCV.notify_one();
            decodedFrames.fetch_add(1);
        }
    }
}

/**
 * @brief 处理线程函数，从处理队列获取视频帧进行预处理（例如：格式转换、图像增强等），然后将其放入解码队列
 * @return 无返回值
 */
void PipeLine::ProcessLoop()
{
    uint64_t localProcessFrameId = 0;
    uint64_t localThrottleDropLogCounter = 0;
    const uint32_t targetInferenceInputFps = Config::Inference::TargetInputFps;
    const auto minInferenceInputIntervalUs =
        targetInferenceInputFps > 0
            ? std::chrono::microseconds(1000000 / targetInferenceInputFps)
            : std::chrono::microseconds(0);
    auto lastInferencePushTime = std::chrono::steady_clock::time_point{};

    while (!stop.load())
    {
        if (paused.load())
        {
            std::unique_lock<std::mutex> lock(processQueueMutex);
            processQueueCV.wait(lock, [this]
                                { return !paused.load() || stop.load(); });
            if (stop.load())
                break;
        }
        // 1. 从处理队列获取一帧数据
        const IoFd *inputDesc = nullptr;
        {
            std::unique_lock<std::mutex> lock(processQueueMutex);
            processQueueCV.wait(lock, [this]
                                { return !processQueue.empty() || stop.load(); });
            if (!processQueue.empty())
            {
                inputDesc = processQueue.front();
                processQueue.pop_front();
            }
            else
            {
                if (stop.load())
                    break;
                else
                    continue;
            }
        }

        // 2. NV12 转 BGR
        cv::Mat image;
        if (inputDesc->base && inputDesc->width > 0 && inputDesc->height > 0)
        {
            uint32_t stride = inputDesc->horStride > 0 ? inputDesc->horStride : inputDesc->width;
            uint32_t frameHeight = inputDesc->verStride > 0 ? inputDesc->verStride : inputDesc->height;
            
            size_t yPlaneSize = stride * frameHeight;
            size_t uvPlaneSize = stride * frameHeight / 2;
            size_t totalNv12Size = yPlaneSize + uvPlaneSize;
            
            if (inputDesc->size < totalNv12Size)
            {
                LOG_ERROR("PIPELINE", "Input buffer size (", inputDesc->size, 
                          ") is smaller than required NV12 size (", totalNv12Size, ")");
            }
            else
            {
                cv::Mat nv12WithStride(frameHeight + frameHeight / 2, stride, CV_8UC1, inputDesc->base);
                cv::Mat bgrWithStride;
                cv::cvtColor(nv12WithStride, bgrWithStride, cv::COLOR_YUV2BGR_NV12);
                image = bgrWithStride(cv::Rect(0, 0, inputDesc->width, inputDesc->height));
            }
        }

        ++localProcessFrameId;
        const int limeInterval = std::max(1, Config::PreProcessing::LIMEFrameInterval);
        const bool runLimeThisFrame = (localProcessFrameId % static_cast<uint64_t>(limeInterval) == 0);

        if (!image.empty() && enableLime && runLimeThisFrame)
        {
            try
            {
                cv::Mat resized;
                cv::resize(image, resized, cv::Size(Config::PreProcessing::LIMEInputWidth, Config::PreProcessing::LIMEInputHeight));
                lime::LimeEnhance enhancer(resized);
                enhancer.SetAlpha(limeAlpha);
                enhancer.SetBeta(Config::PreProcessing::LIMEBeta);
                resized = enhancer.Enhance(resized);
                cv::resize(resized, image, cv::Size(inputDesc->width, inputDesc->height));
            }
            catch (const cv::Exception &e)
            {
                LOG_ERROR("PIPELINE", "LIME enhance failed, skip this frame: ", e.what());
            }
        }

        if (image.empty())
        {
            LOG_WARNING("PIPELINE", "Empty frame after processing, skip inference");
        }
        else
        {
            bool shouldPushInference = true;
            if (targetInferenceInputFps > 0)
            {
                auto now = std::chrono::steady_clock::now();
                if (lastInferencePushTime.time_since_epoch().count() != 0 &&
                    (now - lastInferencePushTime) < minInferenceInputIntervalUs)
                {
                    droppedFrames.fetch_add(1);
                    shouldPushInference = false;
                    ++localThrottleDropLogCounter;
                    if (localThrottleDropLogCounter % 120 == 0)
                    {
                        LOG_WARNING("PIPELINE", "Inference input throttled by TargetInputFps=", targetInferenceInputFps,
                                    ", dropped frames total=", droppedFrames.load());
                    }
                }
                else
                {
                    lastInferencePushTime = now;
                }
            }

            if (shouldPushInference)
            {
                {
                    std::lock_guard<std::mutex> lock(inferenceQueueMutex);
                    if (inferenceQueue.size() > Config::Pipeline::MaxQueueDepth)
                    {
                        inferenceQueue.pop_front();
                        droppedFrames.fetch_add(1);
                        LOG_WARNING("PIPELINE", "Inference queue is full, dropping oldest frame");
                    }
                    inferenceQueue.emplace_back(std::move(image));
                }
                inferenceQueueCV.notify_one();
                processedFrames.fetch_add(1);
            }
        }

        if (needDecode)
        {
            decoder.QueueOutputForRecycle(inputDesc);
        }
        else if (inputDesc->cameraBuffer != nullptr)
        {
            FrameDescription *frameDes = static_cast<FrameDescription*>(inputDesc->cameraBuffer);
            camera.RequeueFrame(frameDes);
            if (inputDesc->allocSize > 0 && inputDesc->base)
            {
                delete[] static_cast<uint8_t*>(inputDesc->base);
            }
            delete inputDesc;
        }
        else
        {
            LOG_ERROR("PIPELINE", "cameraBuffer is nullptr! Cannot requeue V4L2 buffer");
        }
    }
}

/**
 * @brief 推理线程函数，从推理队列获取图像进行车道线检测推理，推理结果与原始图像一起放入显示队列
 * @return 无返回值
 */
void PipeLine::InferenceLoop()
{
    while (!stop.load())
    {
        if (paused.load())
        {
            std::unique_lock<std::mutex> lock(inferenceQueueMutex);
            inferenceQueueCV.wait(lock, [this]
                                  { return !paused.load() || stop.load(); });
            if (stop.load())
                break;
        }

        // 1. 从推理队列获取一帧数据
        cv::Mat image;
        {
            std::unique_lock<std::mutex> lock(inferenceQueueMutex);
            inferenceQueueCV.wait(lock, [this]
                                  { return !inferenceQueue.empty() || stop.load(); });
            if (!inferenceQueue.empty())
            {
                image = inferenceQueue.front();
                inferenceQueue.pop_front();
            }
            else 
            {
                if (stop.load())
                    break;
                else
                    continue;
            }
        }

        if (image.empty())
        {
            LOG_ERROR("PIPELINE", "Received empty image for inference");
            continue;
        }


        // 2. 进行车道线检测推理
        SegmentationResult segResult = laneDetector.Detect(image);
        if (!segResult.valid)
        {
            LOG_ERROR("PIPELINE", "Lane detection failed for the current frame");
            continue;
        }

        // 3. 将推理结果与原始图像一起放入显示队列
        cv::Mat laneOverlay = laneDetector.DrawLaneOverlay(image, segResult);
        {
            std::lock_guard<std::mutex> lock(displayQueueMutex);
            if (Config::Pipeline::DisplayLatestOnly)
            {
                displayQueue.clear();
            }
            else if (displayQueue.size() > Config::Pipeline::MaxQueueDepth)
            {
                displayQueue.pop_front();
                LOG_WARNING("PIPELINE", "Display queue is full, dropping oldest frame");
            }
            displayQueue.emplace_back(std::move(laneOverlay), std::move(segResult));
        }
        displayQueueCV.notify_one(); // 通知显示线程有新帧可处理
        inferenceFrames.fetch_add(1);
    }
}

/**
 * @brief 显示线程函数，从显示队列获取图像和车道线检测结果进行显示，同时调用帧回调函数将结果传递给外部
 * @return 无返回值
 */
void PipeLine::DisplayLoop()
{
    while (!stop.load())
    {
        if (paused.load())
        {
            std::unique_lock<std::mutex> lock(displayQueueMutex);
            displayQueueCV.wait(lock, [this]
                                { return !paused.load() || stop.load(); });
            if (stop.load())
                break;
        }

        // 1. 从显示队列获取一帧数据
        std::pair<cv::Mat, SegmentationResult> displayData;
        {
            std::unique_lock<std::mutex> lock(displayQueueMutex);
            displayQueueCV.wait(lock, [this]
                                { return !displayQueue.empty() || stop.load(); });
            if (!displayQueue.empty())
            {
                displayData = std::move(displayQueue.front());
                displayQueue.pop_front();
            }
            else
            {
                if (stop.load())
                    break;
                else
                    continue;
            }
        }

        // 2. 调用帧回调函数将结果传递给外部
        if (frameCallback && !displayData.first.empty())
        {
            frameCallback(displayData.first, displayData.second);
        }

        displayedFrames.fetch_add(1);
    }
}

/**
 * @brief 请求所有线程停止，设置停止标志并通知所有线程
 * @return 无返回值
 */
void PipeLine::RequestStop()
{
    stop.store(true);
    NotifyAllThreads();
}

/**
 * @brief 通知所有线程，唤醒所有等待条件变量的线程，确保它们能够及时响应停止请求
 * @return 无返回值
 */
void PipeLine::NotifyAllThreads()
{
    captureQueueCV.notify_all();
    processQueueCV.notify_all();
    decodeQueueCV.notify_all();
    inferenceQueueCV.notify_all();
    displayQueueCV.notify_all();
}
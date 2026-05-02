#include "PipeLine.h"
#include "Logger.hpp"
#include "Config.h"

#include <csignal>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>

std::atomic<bool> shouldExit(false);

#ifndef USE_QT
void SignalHandler(int signal)
{
    if (signal == SIGINT)
    {
        shouldExit = true;
    }
}
#endif

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d <device>    Camera device path (default: /dev/video0)" << std::endl;
    std::cout << "  -w <width>     Capture width (default: 1920)" << std::endl;
    std::cout << "  -h <height>    Capture height (default: 1080)" << std::endl;
    std::cout << "  -f <fps>       Capture FPS (default: 30)" << std::endl;
    std::cout << "  -m <model>     Model path (default: models/lane_detection.rknn)" << std::endl;
    std::cout << "  -lime          Enable LIME enhancement (default: disabled)" << std::endl;
    std::cout << "  -l             Enable LIME enhancement (alias of -lime)" << std::endl;
    std::cout << "  --no-lime      Disable LIME enhancement" << std::endl;
    std::cout << "  --help         Show this help message" << std::endl;
}

// 主函数入口
int main(int argc, char *argv[])
{
    std::string device = Config::Camera::CameraPath;
    int width = Config::Camera::CameraWidth;
    int height = Config::Camera::CameraHeight;
    int fps = Config::Camera::CameraFPS;
    std::string model = Config::Inference::ModelPath;
    bool enableLIME = Config::PreProcessing::EnableLIME;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-d" && i + 1 < argc)
        {
            device = argv[++i];
        }
        else if (arg == "-w" && i + 1 < argc)
        {
            width = std::stoi(argv[++i]);
        }
        else if (arg == "-h" && i + 1 < argc)
        {
            height = std::stoi(argv[++i]);
        }
        else if (arg == "-f" && i + 1 < argc)
        {
            fps = std::stoi(argv[++i]);
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            model = argv[++i];
        }
        else if (arg == "-lime" || arg == "-l" || arg == "--lime")
        {
            enableLIME = true;
        }
        else if (arg == "--no-lime" || arg == "--no_lime")
        {
            enableLIME = false;
        }
        else if (arg == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    LOG_INFO("MAIN", "Starting application with device: ", device, ", width: ", width, ", height: ", height,
             ", fps: ", fps, ", model: ", model, ", LIME: ", enableLIME ? "enabled" : "disabled");

    std::signal(SIGINT, SignalHandler); // 注册信号处理函数

    // 2. 构建窗口
    cv::namedWindow("Lane Detection", cv::WINDOW_NORMAL);
    cv::resizeWindow("Lane Detection", 1280, 720);
    
    // 3. 定义帧回调函数
    auto frameCallBack = [](const cv::Mat &frame, const SegmentationResult &result)
    {
        static int frameCount = 0;
        frameCount++;
        if (!frame.empty())
        {
            cv::Mat displayFrame;
            if (frame.cols != static_cast<int>(Config::Display::DisplayWidth) ||
                frame.rows != static_cast<int>(Config::Display::DisplayHeight))
            {
                cv::resize(frame, displayFrame, cv::Size(Config::Display::DisplayWidth, Config::Display::DisplayHeight));
            }
            else
            {
                displayFrame = frame;
            }
            cv::imshow("Lane Detection", displayFrame);
        }

        if (frameCount % 30 == 0)
        {
            LOG_INFO("CALLBACK", "Processed frame ", frameCount, ", valid: ", result.valid, ", lane count: ", result.lanePoints.size());
        }
    };

    PipeLine pipeLine;
    pipeLine.SetFrameCallback(frameCallBack);

    // 4. 初始化并且启动pipeLine
    if (pipeLine.Init(device, width, height, fps, model, enableLIME) != 0)
    {
        LOG_ERROR("MAIN", "Failed to initialize pipeline");
        return -1;
    }

    LOG_INFO("MAIN", "Pipeline initialized successfully, starting...");
    pipeLine.Start(); // 启动管道

    // 5. 主循环，等待退出信号
    while (!shouldExit)
    {
        static auto lastTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        // 每隔5秒打印一次管道状态
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count() >= 5)  
        {
            auto pipelineStatus = pipeLine.GetPipelineStatus();
            LOG_INFO("MAIN", "Pipeline status: captured=", pipelineStatus.capturedFrames, 
                     ", processed=", pipelineStatus.processedFrames, 
                     ", dropped=", pipelineStatus.droppedFrames,
                     ", decoded=", pipelineStatus.decodedFrames,
                     ", inference=", pipelineStatus.inferenceFrames, 
                     ", fps=", pipelineStatus.processingFps);
            lastTime = now;
        }

        int key = cv::waitKey(1);
        if (key == 'q' || key == 27)
        {
            LOG_INFO("MAIN", "Exit signal received, stopping pipeline...");
            shouldExit = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 通过延时减少CPU占用
    }

    LOG_INFO("MAIN", "Stopping pipeline...");
    pipeLine.Stop();
    cv::destroyAllWindows();
    LOG_INFO("MAIN", "Application exited gracefully");
    return 0;
}
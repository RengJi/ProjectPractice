#pragma once

#include "DataType.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <rknn_api.h>

// 车道线检测结果
struct SegmentationResult
{
    cv::Mat mask;                                   // 原始车道线分割掩码
    cv::Mat coloredMask;                            // 彩色可视化掩码
    std::vector<std::vector<cv::Point>> lanePoints; // 每条车道线的点集
    std::vector<int> laneTypes;                     // 每条轮廓对应的类别 ID
    bool valid = false;                             // 结果是否有效
    int64_t timeStampUs = -1;                       // 结果生成的时间戳，单位为微秒
};

class LaneDetector
{
private:
    int LoadModel(const std::string& model_path);
    void PreProcess(const cv::Mat& input_image, cv::Mat& input_tensor);
    void PostProcess(const std::vector<rknn_output> outputs, SegmentationResult& result, int original_width, int original_height);
    cv::Mat ColorMap(cv::Mat& mask);
    void extractLanePoints(const cv::Mat& mask, SegmentationResult& result);
public:
    LaneDetector();
    ~LaneDetector();

    int Init(const std::string& model_path);
    SegmentationResult Detect(const cv::Mat& input_image);
    cv::Mat DrawLaneOverlay(const cv::Mat& input_image, const SegmentationResult& result);

    void SetConfidenceThreshold(float threshold) { confidenceThreshold = threshold; }
    void SetInputSize(int width, int height) { inputWidth = width; inputHeight = height; }  
    void SetClassNum(int num) { classNum = num; }

private:
    rknn_context rknnCtx;
    rknn_input_output_num ioNum;
    std::vector<rknn_tensor_attr> inputAttrs;
    std::vector<rknn_tensor_attr> outputAttrs;
    std::vector<void*> inputBuffers;
    std::vector<void*> outputBuffers;
    int inputWidth = 640;
    int inputHeight = 360;
    int outputWidth = 0;
    int outputHeight = 0;
    int classNum = 2;
    float confidenceThreshold = 0.5f;
    bool initialized = false;

    const cv::Scalar laneColors[8] = {
        cv::Scalar(0, 0, 0),        // 黑色  
        cv::Scalar(0, 0, 255),      // 红色  
        cv::Scalar(0, 255, 0),      // 绿色  
        cv::Scalar(255, 0, 0),      // 蓝色  
        cv::Scalar(0, 255, 255),    // 黄色  
        cv::Scalar(255, 0, 255),    // 洋红色  
        cv::Scalar(255, 255, 0),    // 青色  
        cv::Scalar(128, 128, 128)   // 灰色 
    };
};
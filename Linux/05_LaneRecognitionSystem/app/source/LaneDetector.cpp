
#include "LaneDetector.h"
#include "Logger.hpp"
#include <algorithm>
#include <fstream>
#include <cmath>

LaneDetector::LaneDetector() = default;

LaneDetector::~LaneDetector()
{
    if (initialized)
    {
        rknn_destroy(rknnCtx);
        for (size_t i = 0; i < inputBuffers.size(); ++i)
        {
            free(inputBuffers[i]);
        }
        for (size_t i = 0; i < outputBuffers.size(); ++i)
        {
            free(outputBuffers[i]);
        }
    }
}

/**
 * @brief 初始化车道线检测器，加载 RKNN 模型并准备输入输出缓冲区
 * @param model_path 模型文件路径
 * @return 0 表示成功，非 0 表示失败
 */
int LaneDetector::Init(const std::string &model_path)
{
    return LoadModel(model_path);
}

/**
 * @brief 执行车道线检测，输入图像经过预处理后送入 RKNN 模型进行推理，最后对输出结果进行后处理生成车道线分割掩码和彩色可视化图像，并提取车道线轮廓点集
 * @param input_image 输入图像，要求为 BGR 格式的 OpenCV Mat 对象
 * @return 车道线检测结果，包括车道线分割掩码、彩色可视化图像、车道线轮廓点集等
 */
SegmentationResult LaneDetector::Detect(const cv::Mat &input_image)
{
    SegmentationResult result;
    if (!initialized)
    {
        LOG_ERROR("LaneDetector", "LaneDetector is not initialized. Please call Init() before Detect().");
        return result;
    }
    if (input_image.empty())
    {
        LOG_ERROR("LaneDetector", "Input image is empty.");
        return result;
    }
    // 1. 预处理输入图像并设置 RKNN 输入张量
    cv::Mat inputTensor;
    PreProcess(input_image, inputTensor);

    std::vector<rknn_input> inputs(ioNum.n_input);
    for (uint32_t i = 0; i < ioNum.n_input; ++i)
    {
        inputs[i].index = i;
        inputs[i].buf = inputBuffers[i];
        inputs[i].size = inputAttrs[i].size;
        inputs[i].type = RKNN_TENSOR_UINT8;
        inputs[i].fmt = RKNN_TENSOR_NHWC;
    }

    int hwcSize = inputWidth * inputHeight * 3;         // 输入张量的 HWC 大小
    if (inputTensor.empty() || inputTensor.data == nullptr)
    {
        LOG_ERROR("RKNN", "PreProcess generated empty input tensor");
        return result;
    }
    memcpy(inputBuffers[0], inputTensor.data, hwcSize); // 将预处理后的输入图像数据复制到输入缓冲区

    int ret = rknn_inputs_set(rknnCtx, ioNum.n_input, inputs.data());
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to set RKNN inputs, error code: ", ret);
        return result;
    }


    // 2. 运行 RKNN 推理
    ret = rknn_run(rknnCtx, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to run RKNN inference, error code: ", ret);
        return result;
    }

    // 3. 获取 RKNN 输出张量数据
    std::vector<rknn_output> outputs(ioNum.n_output);
    for (uint32_t i = 0; i < ioNum.n_output; ++i)
    {
        outputs[i].want_float = 0;      // 不需要 RKNN 进行数据转换，直接获取原始输出数据
        outputs[i].is_prealloc = 1;     // 使用预分配的输出缓冲区
        outputs[i].index = i;
        outputs[i].buf = outputBuffers[i];
        outputs[i].size = outputAttrs[i].size;
    }

    ret = rknn_outputs_get(rknnCtx, ioNum.n_output, outputs.data(), nullptr);
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to get RKNN outputs, error code: ", ret);
        return result;
    }

    // 4. 后处理 RKNN 输出数据，生成车道线分割掩码和彩色可视化图像，并提取车道线轮廓点集
    PostProcess(outputs, result, input_image.cols, input_image.rows);

    // 5. 释放 RKNN 输出张量数据
    rknn_outputs_release(rknnCtx, ioNum.n_output, outputs.data());
    result.timeStampUs = cv::getTickCount() / cv::getTickFrequency() * 1e6; // 记录结果生成的时间戳，单位为微秒
    return result;
}

/**
 * @brief 在输入图像上绘制车道线分割结果的彩色叠加图像
 * @param input_image 输入图像，要求为 BGR 格式的 OpenCV Mat 对象
 * @param result 车道线检测结果，包含车道线分割掩码和彩色可视化图像等
 * @return 绘制了车道线分割结果的彩色叠加图像
 */
cv::Mat LaneDetector::DrawLaneOverlay(const cv::Mat &input_image, const SegmentationResult &result)
{
    cv::Mat overlay = input_image.clone();
    if (!result.valid || result.coloredMask.empty())
    {
        LOG_WARNING("LaneDetector", "Invalid segmentation result. Returning original image without overlay.");
        return overlay;
    }

    cv::addWeighted(overlay, 0.7, result.coloredMask, 0.3, 0, overlay); // 将彩色掩码与原始图像进行加权叠加

    for (size_t i = 0; i < result.lanePoints.size(); ++i)
    {
        const auto &points = result.lanePoints[i];
        if (points.size() >= 10)
        {
            cv::polylines(overlay, points, false, laneColors[result.laneTypes[i] % 8], 2); // 根据车道线类别使用不同颜色绘制轮廓线
        }
    }
    return overlay;
}

/**
 * @brief 加载 RKNN 模型并初始化上下文
 * @param model_path 模型文件路径
 * @return 0 表示成功，非 0 表示失败
 */
int LaneDetector::LoadModel(const std::string &model_path)
{
    // 1. 加载模型文件
    std::ifstream modelFile(model_path, std::ios::binary | std::ios::ate);
    if (!modelFile.is_open())
    {
        LOG_ERROR("RKNN", "Failed to open model file: ", model_path);
        return -1;
    }

    // 2. 获取模型文件大小并读取模型数据
    size_t modelSize = modelFile.tellg(); // 获取模型文件大小
    modelFile.seekg(0, std::ios::beg);    // 将文件指针移回文件开头

    std::vector<char> modelData(modelSize);
    modelFile.read(modelData.data(), modelSize);
    modelFile.close();

    int ret = rknn_init(&rknnCtx, modelData.data(), modelSize, 0, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to initialize RKNN context, error code: ", ret);
        return -1;
    }

    // 3. 查询版本
    rknn_sdk_version version;
    ret = rknn_query(rknnCtx, RKNN_QUERY_SDK_VERSION, &version, sizeof(version));
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to query RKNN SDK version, error code: ", ret);
        return -1;
    }
    LOG_INFO("RKNN", "RKNN SDK Version: ", version.api_version, ", Driver Version: ", version.drv_version);

    // 4. 查询输入输出信息
    ret = rknn_query(rknnCtx, RKNN_QUERY_IN_OUT_NUM, &ioNum, sizeof(ioNum));
    if (ret < 0)
    {
        LOG_ERROR("RKNN", "Failed to query RKNN input/output number, error code: ", ret);
        return -1;
    }

    LOG_INFO("RKNN", "Model Input Num: ", ioNum.n_input, ", Output Num: ", ioNum.n_output);

    // 5. 查询输入输出张量属性 
    inputAttrs.resize(ioNum.n_input);
    for (uint32_t i = 0; i< ioNum.n_input; ++i)
    {
        inputAttrs[i].index = i;
        ret = rknn_query(rknnCtx, RKNN_QUERY_INPUT_ATTR, &inputAttrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            LOG_ERROR("RKNN", "Failed to query RKNN input tensor attributes for input ", i, ", error code: ", ret);
            return -1;
        }

        if (inputAttrs[i].n_dims != 4) // 期望输入张量为 4 维（NCHW 或 NHWC）
        {
            LOG_ERROR("RKNN", "Unexpected input tensor dimensions for input ", i, ": expected 4, got ", inputAttrs[i].n_dims);
            return -1;
        }
        else
        {
            if (inputAttrs[i].fmt == RKNN_TENSOR_NHWC)
            {
                inputHeight = inputAttrs[i].dims[1];
                inputWidth = inputAttrs[i].dims[2];
            }
            else if (inputAttrs[i].fmt == RKNN_TENSOR_NCHW)
            {
                inputHeight = inputAttrs[i].dims[2];
                inputWidth = inputAttrs[i].dims[3];
            }
            else
            {
                LOG_ERROR("RKNN", "Unsupported input tensor format for input ", i, ": ", inputAttrs[i].fmt);
                return -1;
             
            }
        }
    }

    outputAttrs.resize(ioNum.n_output);
    for (uint32_t i = 0; i < ioNum.n_output; ++i)
    {
        outputAttrs[i].index = i;
        ret = rknn_query(rknnCtx, RKNN_QUERY_OUTPUT_ATTR, &outputAttrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            LOG_ERROR("RKNN", "Failed to query RKNN output tensor attributes for output ", i, ", error code: ", ret);
            return -1;
        }

        if (outputAttrs[i].n_dims != 4)
        {
            LOG_ERROR("RKNN", "Unexpected output tensor dimensions for output ", i, ": expected 4, got ", outputAttrs[i].n_dims);
            return -1;
        }
        else
        {
            if (outputAttrs[i].fmt == RKNN_TENSOR_NHWC)
            {
                outputHeight = outputAttrs[i].dims[1];
                outputWidth = outputAttrs[i].dims[2];
                classNum = outputAttrs[i].dims[3];
            }
            else if (outputAttrs[i].fmt == RKNN_TENSOR_NCHW)
            {
                outputHeight = outputAttrs[i].dims[2];
                outputWidth = outputAttrs[i].dims[3];
                classNum = outputAttrs[i].dims[1];
            }
            else
            {
                LOG_ERROR("RKNN", "Unsupported output tensor format for output ", i, ": ", outputAttrs[i].fmt);
                return -1;
            }
        }
    }

    LOG_INFO("RKNN", "Model Input Size: ", inputWidth, "x", inputHeight, ", Output Size: ", outputWidth, "x", outputHeight, ", Class Num: ", classNum);

    // 6. 分配输入输出缓冲区
    inputBuffers.resize(ioNum.n_input, nullptr);
    for (uint32_t i = 0; i < ioNum.n_input; ++i)
    {
        inputBuffers[i] = malloc(inputAttrs[i].size);
    }

    outputBuffers.resize(ioNum.n_output, nullptr);
    for (uint32_t i = 0; i < ioNum.n_output; ++i)
    {
        outputBuffers[i] = malloc(outputAttrs[i].size);
    }
    
    initialized = true;
    LOG_INFO("RKNN", "RKNN model loaded and initialized successfully");
    return 0;
}

/**
 * @brief 预处理输入图像，将其调整为模型输入大小并转换为 RGB 格式
 * @param input_image 原始输入图像
 * @param input_tensor 预处理后的输入张量，大小应为模型输入要求的大小
 * @return 无
 */
void LaneDetector::PreProcess(const cv::Mat &input_image, cv::Mat &input_tensor)
{
    cv::Mat resizedImage;
    cv::resize(input_image, resizedImage, cv::Size(inputWidth, inputHeight));
    cv::cvtColor(resizedImage, input_tensor, cv::COLOR_BGR2RGB);
}

/**
 * @brief 后处理 RKNN 模型的输出，生成车道线分割掩码和彩色可视化图像，并提取车道线轮廓点集
 * @param outputs RKNN 模型的输出张量数组
 * @param result 存储后处理结果的结构体，包括车道线分割掩码、彩色可视化图像、车道线轮廓点集等
 * @param original_width 原始输入图像的宽度，用于将掩码调整回原始大小
 * @param original_height 原始输入图像的高度，用于将掩码调整回原始大小
 * @return 无
 */
void LaneDetector::PostProcess(const std::vector<rknn_output> outputs, SegmentationResult &result, int original_width, int original_height)
{
    cv::Mat mask(outputHeight, outputWidth, CV_8UC1);
    size_t outputSize = outputWidth * outputHeight * classNum;
    std::vector<float> outputData(outputSize);


    // 1. 根据输出张量的数据类型进行反量化处理，将 int8/uint8 数据转换为 float 数据
    if (outputAttrs[0].type == RKNN_TENSOR_INT8)
    {
        const int8_t *int8Data = static_cast<const int8_t *>(outputs[0].buf);
        float scale = outputAttrs[0].scale;
        int zeroPoint = outputAttrs[0].zp;

        for (size_t i = 0; i < outputSize; ++i)
        {
            outputData[i] = (int8Data[i] - zeroPoint) * scale;
        }
    }
    else if (outputAttrs[0].type == RKNN_TENSOR_UINT8)
    {
        const uint8_t *uint8Data = static_cast<const uint8_t *>(outputs[0].buf);
        float scale = outputAttrs[0].scale;
        int zeroPoint = outputAttrs[0].zp;

        for (size_t i = 0; i < outputSize; ++i)
        {
            outputData[i] = (uint8Data[i] - zeroPoint) * scale;
        }
    }
    else 
    {
        const float *floatData = static_cast<const float *>(outputs[0].buf);
        memcpy(outputData.data(), floatData, outputSize * sizeof(float));
    }

    // 2. 根据输出张量的格式（NHWC 或 NCHW）解析输出数据，找到每个像素的最大类别 ID，并将其写入掩码图像
    for (int h = 0; h < outputHeight; ++h)
    {
        for (int w = 0; w < outputWidth; ++w)
        {
            int maxClassId = 0;
            float maxVal;
            if (outputAttrs[0].fmt == RKNN_TENSOR_NHWC)
            {
                maxVal = outputData[h * outputWidth * classNum + w * classNum + 0];
                for (int c = 1; c < classNum; ++c)
                {
                    float val = outputData[h * outputWidth * classNum + w * classNum + c];
                    if (val > maxVal)
                    {
                        maxVal = val;
                        maxClassId = c;
                    }
                }
            }
            else if (outputAttrs[0].fmt == RKNN_TENSOR_NCHW)
            {
                maxVal = outputData[0 * outputHeight * outputWidth + h * outputWidth + w];
                for (int c = 1; c < classNum; ++c)
                {
                    float val = outputData[c * outputHeight * outputWidth + h * outputWidth + w];
                    if (val > maxVal)
                    {
                        maxVal = val;
                        maxClassId = c;
                    }
                }
            }
            else
            {
                // 不支持的输出格式
                LOG_ERROR("RKNN", "Unsupported output tensor format: ", outputAttrs[0].fmt);
                return;
            }
            mask.at<uint8_t>(h, w) = static_cast<uint8_t>(maxClassId); // 将类别 ID 写入掩码图像
        }
    }

    // 3. 将掩码图像调整回原始图像大小，并存储在结果结构体中，同时生成彩色可视化掩码并提取车道线轮廓点集
    cv::resize(mask, mask, cv::Size(original_width, original_height), 0, 0, cv::INTER_NEAREST); // 将掩码图像调整回原始图像大小
    result.mask = mask;
    result.coloredMask = ColorMap(mask);
    extractLanePoints(mask, result);
    result.valid = true;
}

/**
 * @brief 将车道线分割掩码转换为彩色可视化图像
 * @param mask 车道线分割掩码，每个像素的值表示类别 ID
 * @return 彩色可视化图像，车道线以不同颜色显示，背景为白色
 */
cv::Mat LaneDetector::ColorMap(cv::Mat &mask)
{
    cv::Mat colorMask(mask.size(), CV_8UC3);
    for (int h = 0; h < mask.rows; ++h)
    {
        for (int w = 0; w < mask.cols; ++w)
        {
            int classId = mask.at<uchar>(h, w); // 获取类别 ID
            if (classId > 0 && classId <= 8)
            {
                colorMask.at<cv::Vec3b>(h, w) = cv::Vec3b(laneColors[classId - 1][0], laneColors[classId - 1][1], laneColors[classId - 1][2]); // 根据类别 ID 设置颜色
            }
            else
            {
                colorMask.at<cv::Vec3b>(h, w) = cv::Vec3b(255, 255, 255); // 背景颜色（白色）
            }
        }
    }
    return colorMask;
}

/**
 * @brief 从车道线分割掩码中提取车道线轮廓点集
 * @param mask 车道线分割掩码，每个像素的值表示类别 ID
 * @param result 车道线检测结果结构体，提取的车道线轮廓点集将存储在 result.lanePoints 中，对应的类别 ID 将存储在 result.laneTypes 中
 * @return 无
 */
void LaneDetector::extractLanePoints(const cv::Mat &mask, SegmentationResult &result)
{
    for (int i = 0; i < classNum; ++i)
    {
        cv::Mat binary;
        cv::compare(mask, i, binary, cv::CMP_EQ);   // 创建二值图像，车道线像素为255，其他为0

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // 查找轮廓

        for (auto &contour : contours)
        {
            if (contour.size() >= 10)       // 过滤掉过小的轮廓，避免噪声干扰
            {
                result.lanePoints.push_back(contour);
                result.laneTypes.push_back(i);          // 记录对应的类别 ID
            }
        }
    }
}
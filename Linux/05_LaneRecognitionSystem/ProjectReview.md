# 项目回顾
## 项目总体PipeLine
```txt
imx415 -> MIPI CSI-2 -> ISP -> V4L2 -> (压缩码流则使用 MPP 解码，否则不直接跳过) -> (开始Lime增强则使用Lime做增强预处理，否则跳过) -> 传入 ppseg 模型 -> 后处理检测结果绘制 -> (需要推送到需要编码的平台就使用 MPP编码)项目直接使用 OPENCV 输出画面
```
## 关键模块
### V4L2
#### 关键类
```c
class V4L2Camera
{
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
}
```

####关键功能函数
```c
// 打开摄像头
int V4L2Camera::Open(const std::string &devicePath)
{
    // 1. 打开摄像头设备文件
    // 2. 查询能力(是否支持流传输和(多平面)视频流的捕获)
}

// 设置捕获参数
int V4L2Camera::SetCameraFormat()
{
    // 1. 枚举摄像头支持的格式，按照 NV12、NV21、YUYV、MJPEG 的优先级设置摄像头像素格式
    // 2. 设置摄像头的其他参数：分辨率、捕获模式和扫描模式
    // 3. 设置流参数：帧率
    // 4. 使用配置结构体读取配置验证设置
}

// 分配缓冲区
int V4L2Camera::InitBuffer()
{
    // 1. 请求摄像头缓冲区，并检查请求的缓冲区数量是否满足要求
    // 2. 循环映射缓冲区(优先使用内核导出的 EXP，导出失败则使用用户空间提供 fd 区域)
}

// 读取可读帧数据
int V4L2Camera::DequeueFrame(unsigned int &index, unsigned int &buffer_size, size_t max_buffer_size)
{
    // 1. 配置好缓冲区结构体
    // 2. select 或取可读通知
}
```

### Mpp 编解码
#### 关键类
```c
// 解码器
class MppDecoder
{
    MppCtx *ctx = nullptr;
    MppApi *mpi = nullptr;
    MppBufferGroup *group = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t horStride = 0;  // 水平步长
    uint32_t verStride = 0;  // 垂直步长
    size_t outSize = 0;

    std::mutex taskMutex; 
    IoFd mppOutputFdList[ResourceLimit::MppOutputBufferCount] = {};                 // MPP输出文件描述符列表
    std::map<int, size_t> fdToIndexMap;                                             // 文件描述符到索引的映射
    std::array<DecodeTaskHolder, ResourceLimit::MppOutputBufferCount> holderPool;   // 解码任务持有者池
    std::deque<DecodeTaskHolder*> availableHolders;                                 // 可用解码任务持有者队列
    std::deque<DecodeTaskHolder*> recycleHolders;                                   // 待回收的解码任务持有者队列
    std::map<const IoFd*, DecodeTaskHolder*> fdToHolderMap;                         // 输入文件描述符到解码任务持有者的映射
    MppBuffer importBuffers[ResourceLimit::MppImportBufferCount] = {};              // MPP导入缓冲区列表
    const IoFd* currentOutputDesc = nullptr;                                        // 当前输出描述符指针
}
// 编码器
class MppEncoder 
{
    MppCtx mppCtx = nullptr;  
    MppApi *mppApi = nullptr;  
    MppEncPrepCfg prepCfg{};    // 预处理配置
    MppEncRcCfg rcCfg{};        // 码率控制配置
    MppEncCodecCfg codecCfg{};  // 编码器配置
    uint32_t width = 0;         // 视频宽度
    uint32_t height = 0;        // 视频高度
    uint32_t horStride = 0;     // 水平步幅
    uint32_t verStride = 0;     // 垂直步幅
    uint32_t fps = 30;          // 视频帧率
    uint32_t bitRate = 2000000; // 视频码率
    uint32_t gop = 60;          // 关键帧间隔
    bool packetEos = false;     // 数据包结束标志
    std::mutex encMutex; 
};
```
#### 关键功能函数
```c
// 编码器
// 设置编码参数
int MppEncoder::SetWidthHeight(uint32_t width, uint32_t height, uint32_t hor_stride, uint32_t ver_stride)
{
    // 1. 设置预处理配置
    // 2. 设置码率控制的配置：码率控制模式、码率变换模式、帧率和关键帧间隔之类的
    // 3. 设置编码参数：编码质量、编码等级和熵编码模式
}

// 将需要编码的帧放入缓冲区
int MppEncoder::PutFrame(const IoFd *io)
{
    // 1. 绑定编码器和缓冲区
    // 2. poll 等待编码入后可用，取出一个任务结构体
    // 3. 绑定任务和输出帧
    // 4. 将绑定好需要编码的帧的结构体重新入队
}

// 从缓冲区读入解码帧
int MppEncoder::GetPacket(MppEncPacketResult &result)
{
    // 1. poll 等待有可以读取的编码数据包
    // 2. 使用 packet 从 task 里面构建
    // 3. 使用自定义的 IoFd 结构体从 packet 里面或取信息
    // 4. 释放使用的 packet
}

// 创建缓冲区
int MppDecoder::AllocBufDmaFd(IoFd *desc, size_t size)
{
    // 1. 打开dma_heap设备文件以获取文件描述符
    // 2. 使用ioctl调用DMA_HEAP_IOCTL_ALLOC命令分配DMA缓冲区，并获取分配的文件描述符
}

// 解码
int MppDecoder::Decode(const FrameDescription *frame_desc)
{
    // 1. 使用 V4L2 步骤的帧数据构建输入 packet
    // 2. 获取输入任务并将数据包设置到 task 数据中
    // 3. 处理输出任务，获取解码后的帧并准备输出描述符
    // 4. 获取一个可用的DecodeTaskHolder来持有当前的输出任务信息
    // 5. 将输出帧和缓冲区信息保存到DecodeTaskHolder中(更新 关联结构体信息 和 当前输出描述符指针，供外部访问)
}
```
### Lime 增强
#### 关键类
```c
class LimeEnhance
{
    cv::Mat normalImage;     // 归一化后的图像
    cv::Mat enhancedImage;   // 增强后的图像
    cv::Mat illumInitImage;  // 初始估计的光照图
    cv::Mat dvMatrix;        // 垂直方向的差分矩阵
    cv::Mat dhMatrix;        // 水平方向的差分矩阵
    cv::Mat weightMatrix;    // 权重矩阵
    cv::Mat laplacianMatrix; // 拉普拉斯算子矩阵

    int channels; // 图像通道数
    int rows;     // 图像行数
    int cols;     // 图像列数

    float alpha;         // 正则化系数，控制光照图的平滑程度
    float beta;          // 惩罚因子增长率，控制 ADMM 收敛速度
    float epsilon;       // 收敛阈值基准
    float maxIterations; // 迭代次数阈值
}
``` 

#### 关键功能函数
```c

``` 
### RGA 预处理
#### 关键类
```c
class RgaProcessor
{
    bool initialized = false;
    bool outReady = false;
    std::mutex outMtx;
    std::array<bool, ResourceLimit::RgaOutputBufferCount> outBufferInUse{};
    std::queue<IoFd *> availablePositions;
    IoFd outBuffers[ResourceLimit::RgaOutputBufferCount]{};
}
``` 
#### 关键功能函数
```c
IoFd *RgaProcessor::TransformInternal(const IoFd *src, RgaProcessor::Operation op, uint32_t dst_width, uint32_t dst_height, uint32_t dst_format)
{

}
```
### RKNN 推理
#### 关键类
```c
class LaneDetector
{
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
}
``` 
#### 关键函数
```c
// 加载模型
int LaneDetector::LoadModel(const std::string &model_path)
{
    // 1. 打开模型文件
    // 2. 读取文件信息
    // 3. 查询 SDK 版本和输入输出张量信息
    // 4. 查询输入输出张量信息
    // 5. 分配输入输出缓冲区
}
``` 
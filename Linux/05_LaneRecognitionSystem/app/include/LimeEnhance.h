#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>

namespace lime
{
    class LimeEnhance
    {
    private:
        cv::Mat MatToVector(const cv::Mat &mat);                  // 将图像矩阵转换为一维向量，用于 FFT 变换
        cv::Mat VectorToMat(const cv::Mat &vec);                  // 将一维向量转换回图像矩阵
        cv::Mat GotReal(const cv::Mat &complexMat);               // 从复数矩阵中提取实部
        cv::Mat Derivate(const cv::Mat &matrix);                  // 计算图像的梯度，用于构建差分矩阵
        cv::Mat Dev(int n, int k);                                // 构建 n 维空间中第 k 个方向的差分矩阵
        cv::Mat GetMax(const cv::Mat &matrix);                    // 获取矩阵中的最大值
        cv::Mat IllumOptimization();                              // 进行光照图优化，使用 ADMM 算法求解优化问题
        cv::Mat SolveT(cv::Mat g, cv::Mat z, float u);            // 更新光照图 T
        cv::Mat SolveG(cv::Mat t, cv::Mat z, float u, cv::Mat w); // 更新辅助变量 G
        cv::Mat SolveZ(cv::Mat t, cv::Mat g, float u, cv::Mat z); // 更新对偶变量 Z
        float SolveU(float u);                                    // 更新惩罚因子 U
        float FrobeniusNorm(const cv::Mat &mat);                  // 计算矩阵的 Frobenius 范数，用于判断 ADMM 的收敛性
        void WeightCal();                                         // 计算边缘感知权重矩阵
        void InitIllum(const cv::Mat &src);                       // 初始化光照图，使用高斯模糊对输入图像进行平滑处理
        void FFT(const cv::Mat &input, cv::Mat &output, int opt); // 对输入矩阵进行快速傅里叶变换
        int ReverseBin(int n, int a);                             // 位反转，FFT 蝶形运算需要

        static inline float Compare(float &a, float &b, float &c) // 比较三个值并返回最大值
        {
            return fmax(fmax(a, b), c);
        }

    public:
        explicit LimeEnhance(const cv::Mat &input);     // 构造函数，接受输入图像并进行必要的初始化
        ~LimeEnhance() = default;                       // 默认析构函数
        cv::Mat Enhance(cv::Mat &src);                  // 增强函数，接受输入图像并返回增强后的图像
        void SetAlpha(float a) { alpha = a; }           // 设置正则化系数 alpha
        void SetBeta(float b) { beta = b; }             // 设置惩罚因子增长率 beta

    private:
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
    };
}

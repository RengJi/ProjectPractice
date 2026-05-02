#include "LimeEnhance.h"
#include <arm_neon.h>
#include <omp.h>

namespace lime
{
    LimeEnhance::LimeEnhance(const cv::Mat &input)
    {
        channels = input.channels();    // 获取图像通道数
        alpha = 1.0f;                   // 默认正则化系数
        beta = 2.0f;                    // 默认惩罚因子增长率
        epsilon = 1e-4f;                // 默认收敛阈值基准
        maxIterations = 30.0f;          // 默认迭代次数阈值
    }

    /**
     * @brief 增强函数，接受输入图像并返回增强后的图像，首先进行光照图优化，然后对每个通道进行增强处理，最后合并增强后的通道并返回结果
     * @param src 输入图像，假设为三通道图像
     * @return 返回增强后的图像，类型为 CV_8UC3
     */
    cv::Mat LimeEnhance::Enhance(cv::Mat &src)
    {
        InitIllum(src);                             // 初始化光照图
        cv::Mat T = IllumOptimization();            // 进行光照图优化

        cv::Mat srcFloat;
        src.convertTo(srcFloat, CV_32FC3, 1.0 / 255.0);
        std::vector<cv::Mat> channelsVec;
        cv::split(srcFloat, channelsVec);            // 分离通道

        cv::Mat safeT;
        cv::max(T, 1e-4, safeT);

        cv::Mat b, g, r;
#pragma omp parallel sections
        {
#pragma omp section
            {
                cv::divide(channelsVec[0], safeT, b); // 增强蓝色通道
                cv::threshold(b, b, 0.0, 0.0, 3);   // 对增强后的蓝色通道进行阈值处理，去除负值
            }

#pragma omp section
            {
                cv::divide(channelsVec[1], safeT, g); // 增强绿色通道
                cv::threshold(g, g, 0.0, 0.0, 3);   // 对增强后的绿色通道进行阈值处理，去除负值
            }
#pragma omp section
            {
                cv::divide(channelsVec[2], safeT, r); // 增强红色通道
                cv::threshold(r, r, 0.0, 0.0, 3);   // 对增强后的红色通道进行阈值处理，去除负值
            }
        }

        channelsVec = {b, g, r};
        cv::merge(channelsVec, enhancedImage);              // 合并增强后的通道
        cv::min(enhancedImage, 1.0f, enhancedImage);
        enhancedImage.convertTo(enhancedImage, CV_8U, 255); // 转换为 8 位无符号整数类型
        return enhancedImage;
    }

    /**
     * @brief 将图像矩阵转换为一维向量，假设输入矩阵为单通道浮点数矩阵
     * @param mat 输入的图像矩阵，假设为单通道浮点数矩阵
     * @return 返回一个单行矩阵，其中包含输入图像矩阵的像素值，按照行优先顺序排列
     */
    cv::Mat LimeEnhance::MatToVector(const cv::Mat &mat)
    {
        int col = mat.cols;
        int row = mat.rows;
        cv::Mat vec(1, col * row, CV_32F); // 创建一个单行矩阵，长度为图像的像素总数
        int totalElements = col * row;     // 计算总元素数量
        for (int i = 0; i < totalElements; i += 4)
        {
            float32x4_t pixelVec = vld1q_f32(mat.ptr<float>(0) + i); // 加载4个像素值到 NEON 寄存器
            vst1q_f32(vec.ptr<float>(0) + i, pixelVec);              // 将 NEON 寄存器中的值存储到结果矩阵中
        }

        return vec;
    }

    /**
     * @brief 将一维向量转换回图像矩阵，假设输入矩阵为单行矩阵，长度为图像的像素总数
     * @param vec 输入的一维向量，假设为单行矩阵，长度为图像的像素总数
     * @return 返回一个与输入图像大小相同的矩阵，其中包含输入向量的值，按照行优先顺序排列
     */
    cv::Mat LimeEnhance::VectorToMat(const cv::Mat &vec)
    {
        cv::Mat mat(rows, cols, CV_32F); // 创建一个与输入图像大小相同的矩阵
        for (int i = 0; i < cols; ++i)
        {
            for (int j = 0; j < rows; ++j)
            {
                mat.at<float>(j, i) = vec.at<float>(0, i * rows + j); // 将一维向量中的值按照行优先顺序填充到矩阵中
            }
        }

        return mat;
    }

    /**
     * @brief 从复数矩阵中提取实部，假设输入矩阵的实部和虚部分别存储在相邻的列中
     * @param complexMat 输入的复数矩阵，假设为单行矩阵，其中实部和虚部分别存储在相邻的列中
     * @return 返回一个单行矩阵，其中包含输入复数矩阵的实部
     */
    cv::Mat LimeEnhance::GotReal(const cv::Mat &complexMat)
    {
        int tempCols = complexMat.cols;
        cv::Mat realPort(1, tempCols, CV_32F, cv::Scalar::all(0.0f)); // 初始化实部矩阵
        for (int i = 0; i < tempCols; ++i)
        {
            realPort.at<float>(0, i) = complexMat.at<float>(0, 2 * i); // 提取实部
        }
        return realPort;
    }

    /**
     * @brief 计算图像的梯度，使用预先构建的差分矩阵进行卷积操作
     * @param matrix 输入图像矩阵，假设为单通道浮点数矩阵
     * @return 返回一个矩阵，其中包含输入图像在垂直和水平方向上的梯度信息，垂直梯度和水平梯度分别存储在矩阵的前半部分和后半部分
     */
    cv::Mat LimeEnhance::Derivate(const cv::Mat &matrix)
    {
        cv::Mat v = dvMatrix * matrix; // 计算垂直方向的梯度
        cv::Mat h = matrix * dhMatrix; // 计算水平方向的梯度
        cv::Mat temp;
        cv::vconcat(v, h, temp); // 将垂直和水平方向的梯度合并成一个矩阵
        return temp;
    }

    /**
     * @brief 构建 n 维空间中第 k 个方向的差分矩阵，使用中心差分方法构建差分矩阵
     * @param n 差分矩阵的维度，即输入图像的像素总数
     * @param k 差分矩阵的方向，正值表示超对角线，负值表示次对角线
     * @return 返回一个 n x n 的差分矩阵，其中在第 k 个超对角线或次对角线上设置为 1，其余元素设置为 -1
     */
    cv::Mat LimeEnhance::Dev(int n, int k)
    {
        cv::Mat temp(n, n, CV_32F, cv::Scalar(-1.0f));
        if (k > 0)
        {
            for (int i = 0; i < n - k; ++i)
            {
                temp.at<float>(i, i + k) = 1.0f; // 在第 k 个超对角线上设置为 1
            }
        }
        else
        {
            for (int i = -k; i < n; ++i)
            {
                temp.at<float>(i, i + k) = 1.0f; // 在第 k 个次对角线上设置为 1
            }
        }

        return temp;
    }

    /**
     * @brief 获取矩阵中的最大值，针对每个像素点，取其三个通道的最大值作为该像素点的值
     * @param matrix 输入矩阵，假设为三通道图像
     * @return 返回一个单通道矩阵，其中每个像素点的值为输入矩阵对应像素点三个通道的最大值
     */
    cv::Mat LimeEnhance::GetMax(const cv::Mat &matrix)
    {
        cv::Mat temp(rows, cols, CV_32F, cv::Scalar(0.0)); // 初始化最大值矩阵
        std::vector<cv::Mat> channelsVec;
        cv::Mat imageB, imageG, imageR;
        cv::split(matrix, channelsVec); // 分离通道
        imageB = channelsVec.at(0);
        imageG = channelsVec.at(1);
        imageR = channelsVec.at(2);

#pragma omp parallel sections
        {
#pragma omp section
            {
                for (int i = 0; i < rows / 2; ++i)
                {
                    for (int j = 0; j < cols / 2; j += 4)
                    {
                        float32x4_t bVec = vld1q_f32(imageB.ptr<float>(i) + j);      // 加载蓝色通道的4个像素值
                        float32x4_t gVec = vld1q_f32(imageG.ptr<float>(i) + j);      // 加载绿色通道的4个像素值
                        float32x4_t rVec = vld1q_f32(imageR.ptr<float>(i) + j);      // 加载红色通道的4个像素值
                        float32x4_t maxVec = vmaxq_f32(vmaxq_f32(bVec, gVec), rVec); // 计算4个像素的最大值
                        vst1q_f32(temp.ptr<float>(i) + j, maxVec);                   // 将最大值存储到结果矩阵中
                    }
                }
            }

#pragma omp section
            {
                for (int i = rows / 2; i < rows; ++i)
                {
                    for (int j = 0; j < cols / 2; j += 4)
                    {
                        float32x4_t bVec = vld1q_f32(imageB.ptr<float>(i) + j);      // 加载蓝色通道的4个像素值
                        float32x4_t gVec = vld1q_f32(imageG.ptr<float>(i) + j);      // 加载绿色通道的4个像素值
                        float32x4_t rVec = vld1q_f32(imageR.ptr<float>(i) + j);      // 加载红色通道的4个像素值
                        float32x4_t maxVec = vmaxq_f32(vmaxq_f32(bVec, gVec), rVec); // 计算4个像素的最大值
                        vst1q_f32(temp.ptr<float>(i) + j, maxVec);                   // 将最大值存储到结果矩阵中
                    }
                }
            }

#pragma omp section
            {
                for (int i = 0; i < rows / 2; ++i)
                {
                    for (int j = cols / 2; j < cols; j += 4)
                    {
                        float32x4_t bVec = vld1q_f32(imageB.ptr<float>(i) + j);      // 加载蓝色通道的4个像素值
                        float32x4_t gVec = vld1q_f32(imageG.ptr<float>(i) + j);      // 加载绿色通道的4个像素值
                        float32x4_t rVec = vld1q_f32(imageR.ptr<float>(i) + j);      // 加载红色通道的4个像素值
                        float32x4_t maxVec = vmaxq_f32(vmaxq_f32(bVec, gVec), rVec); // 计算4个像素的最大值
                        vst1q_f32(temp.ptr<float>(i) + j, maxVec);                   // 将最大值存储到结果矩阵中
                    }
                }
            }

#pragma omp section
            {
                for (int i = rows / 2; i < rows; ++i)
                {
                    for (int j = cols / 2; j < cols; j += 4)
                    {
                        float32x4_t bVec = vld1q_f32(imageB.ptr<float>(i) + j);      // 加载蓝色通道的4个像素值
                        float32x4_t gVec = vld1q_f32(imageG.ptr<float>(i) + j);      // 加载绿色通道的4个像素值
                        float32x4_t rVec = vld1q_f32(imageR.ptr<float>(i) + j);      // 加载红色通道的4个像素值
                        float32x4_t maxVec = vmaxq_f32(vmaxq_f32(bVec, gVec), rVec); // 计算4个像素的最大值
                        vst1q_f32(temp.ptr<float>(i) + j, maxVec);                   // 将最大值存储到结果矩阵中
                    }
                }
            }
        }

        return temp;
    }

    /**
     * @brief 进行光照图优化，使用 ADMM 算法求解优化问题，迭代更新光照图 T、辅助变量 G 和对偶变量 Z，直到满足迭代次数阈值或收敛条件
     * @return 返回优化后的光照图 T
     */
    cv::Mat LimeEnhance::IllumOptimization()
    {
        WeightCal();                                              // 计算边缘感知权重矩阵
        cv::Mat T(rows, cols, CV_32F, cv::Scalar::all(0.0f));     // 初始化光照图矩阵
        cv::Mat G(rows * 2, cols, CV_32F, cv::Scalar::all(0.0f)); // 初始化辅助变量矩阵
        cv::Mat Z(rows * 2, cols, CV_32F, cv::Scalar::all(0.0f)); // 初始化对偶变量矩阵
        float u = 1;
        int t = 0;

        while (true)
        {
            T = SolveT(G, Z, u);               // 更新光照图 T
            G = SolveG(T, Z, u, weightMatrix); // 更新辅助变量 G
            Z = SolveZ(T, G, u, Z);            // 更新对偶变量 Z
            u = SolveU(u);                     // 更新惩罚因子 U

            if (t == 0)
            {
                int temp = FrobeniusNorm(Derivate(T) - G);
                maxIterations = static_cast<float>(ceil(2 * log(temp / epsilon))); // 计算迭代次数阈值
            }

            t++; // 迭代次数加1

            if (t >= static_cast<int>(maxIterations) || FrobeniusNorm(Derivate(T) - G) < epsilon)
            {
                break; // 满足迭代次数阈值或收敛条件，退出循环
            }
        }
        return T;
    }

    /**
     * @brief 更新光照图 T，根据 ADMM 的更新规则，计算当前的辅助变量 G 和对偶变量 Z，使用快速傅里叶变换求解优化问题，并进行归一化处理
     * @param g 当前的辅助变量 G
     * @param z 当前的对偶变量 Z
     * @param u 当前的惩罚因子 U
     * @return 返回更新后的光照图 T
     */
    cv::Mat LimeEnhance::SolveT(cv::Mat g, cv::Mat z, float u)
    {
        cv::Mat x = g - z / u; // 计算中间变量 x
        int tempRows = x.rows;

        cv::Mat Xv = x.rowRange(0, rows);             // 提取垂直梯度部分
        cv::Mat Xh = x.rowRange(rows, tempRows);      // 提取水平梯度部分
        cv::Mat temp = dvMatrix * Xv + Xh * dhMatrix; // 计算差分矩阵与梯度的乘积

        cv::Mat numerator;
        cv::Mat denominator;
        cv::Mat tempMat1 = MatToVector(2 * illumInitImage + u * temp); // 计算分子部分的向量表示
        cv::Mat tempMat2 = laplacianMatrix * u;
        FFT(tempMat1, numerator, 1);    // 对分子部分进行 FFT 变换
        FFT(tempMat2, denominator, 1);  // 对分母部分进行 FFT 变换
        denominator = denominator + 2;  // 分母加上常数项
        temp = numerator / denominator; // 计算频域中的结果

        cv::Mat tempT;
        FFT(temp, tempT, -1);
        tempT = GotReal(tempT);

        cv::normalize(tempT, tempT, 0.2, 1, cv::NORM_MINMAX); // 对结果进行归一化处理
        cv::Mat T = VectorToMat(tempT);                       // 将结果转换回图像矩阵形式
        T.convertTo(T, CV_32F);                               // 确保结果为单通道浮点数矩阵
        return T;
    }

    /**
     * @brief 更新辅助变量 G，根据 ADMM 的更新规则，计算当前的光照图 T 和对偶变量 Z，使用软阈值函数进行更新，并考虑边缘感知权重矩阵的影响
     * @param t 当前的光照图 T
     * @param z 当前的对偶变量 Z
     * @param u 当前的惩罚因子 U
     * @param w 边缘感知权重矩阵
     * @return 返回更新后的辅助变量 G
     */
    cv::Mat LimeEnhance::SolveG(cv::Mat t, cv::Mat z, float u, cv::Mat w)
    {
        cv::Mat dt = Derivate(t);       // 计算光照图的梯度
        cv::Mat epsMat = alpha * w / u; // 计算 epsilon 矩阵
        cv::Mat tempMat1 = dt + z / u;  // 计算中间变量 tempMat1

        int tempRows = tempMat1.rows;
        int tempCols = tempMat1.cols;
        cv::Mat tempMat2(tempRows, tempCols, CV_32F); // 初始化结果矩阵
        for (int i = 0; i < tempRows; ++i)
        {
            for (int j = 0; j < tempCols; ++j)
            {
                if (tempMat1.at<float>(i, j) > 0)
                {
                    tempMat2.at<float>(i, j) = 1; // 如果 tempMat1 的值大于 0，则结果矩阵对应位置设置为 1
                }
                else if (tempMat1.at<float>(i, j) < 0)
                {
                    tempMat2.at<float>(i, j) = -1; // 如果 tempMat1 的值小于 0，则结果矩阵对应位置设置为 -1
                }
                else
                {
                    tempMat2.at<float>(i, j) = 0; // 如果 tempMat1 的值等于 0，则结果矩阵对应位置设置为 0
                }
            }
        }

        cv::Mat tempMat3 = cv::max(cv::abs(tempMat1) - epsMat, 0); // 计算 max(|tempMat1| - epsMat, 0)
        cv::Mat tempMat4 = tempMat2.mul(tempMat3);                 // 计算结果矩阵
        return tempMat4;                                           // 返回更新后的辅助变量 G
    }

    /**
     * @brief 更新对偶变量 Z，根据 ADMM 的更新规则，计算光照图 T 的梯度与辅助变量 G 之间的差值，并乘以惩罚因子 U，最后加上当前的对偶变量 Z
     * @param t 当前的光照图 T
     * @param g 当前的辅助变量 G
     * @param u 当前的惩罚因子 U
     * @param z 当前的对偶变量 Z
     * @return 返回更新后的对偶变量 Z
     */
    cv::Mat LimeEnhance::SolveZ(cv::Mat t, cv::Mat g, float u, cv::Mat z)
    {
        cv::Mat dT = Derivate(t); // 计算光照图的梯度
        return z + u * (dT - g);  // 更新对偶变量 Z
    }

    /**
     * @brief 更新惩罚因子 U，根据 ADMM 的更新规则，将当前的惩罚因子 U 乘以增长率 beta，以加快 ADMM 的收敛速度
     * @param u 当前的惩罚因子 U
     * @return 返回更新后的惩罚因子 U
     */
    float LimeEnhance::SolveU(float u)
    {
        return u * beta; // 更新惩罚因子 U，乘以增长率 beta
    }

    /**
     * @brief 计算矩阵的 Frobenius 范数，使用 NEON SIMD 指令进行优化
     * @param mat 输入矩阵，假设为单通道浮点数矩阵
     * @return 返回矩阵的 Frobenius 范数，即矩阵元素的平方和的平方根
     */
    float LimeEnhance::FrobeniusNorm(const cv::Mat &mat)
    {
        int row = mat.rows;
        int col = mat.cols;

        float sum1, sum2, sum3, sum4;

#pragma omp parallel sections
        {
#pragma omp section
            {
                float32x4_t totalSum = vdupq_n_f32(0.0f); // 初始化总和向量
                for (int i = 0; i < row / 2; ++i)
                {
                    for (int j = 0; j < col / 2; j += 4)
                    {
                        float32x4_t vec = vld1q_f32(mat.ptr<float>(i) + j); // 加载4个元素
                        float32x4_t sqVec = vmulq_f32(vec, vec);            // 计算平方
                        totalSum = vaddq_f32(totalSum, sqVec);              // 累加平方和
                    }
                }

                totalSum = vpaddq_f32(totalSum, totalSum);       // 水平加法，得到总和的两个部分
                totalSum = vpaddq_f32(totalSum, totalSum);       // 再次水平加法，得到最终的总和
                sum1 = vget_lane_f32(vget_low_f32(totalSum), 0); // 提取总和的值
            }

#pragma omp section
            {
                float32x4_t totalSum = vdupq_n_f32(0.0f); // 初始化总和向量
                for (int i = 0; i < row / 2; ++i)
                {
                    for (int j = col / 2; j < col; j += 4)
                    {
                        float32x4_t vec = vld1q_f32(mat.ptr<float>(i) + j); // 加载4个元素
                        float32x4_t sqVec = vmulq_f32(vec, vec);            // 计算平方
                        totalSum = vaddq_f32(totalSum, sqVec);              // 累加平方和
                    }
                }

                totalSum = vpaddq_f32(totalSum, totalSum);       // 水平加法，得到总和的两个部分
                totalSum = vpaddq_f32(totalSum, totalSum);       // 再次水平加法，得到最终的总和
                sum2 = vget_lane_f32(vget_low_f32(totalSum), 0); // 提取总和的值
            }

#pragma omp section
            {
                float32x4_t totalSum = vdupq_n_f32(0.0f); // 初始化总和向量
                for (int i = row / 2; i < row; ++i)
                {
                    for (int j = 0; j < col / 2; j += 4)
                    {
                        float32x4_t vec = vld1q_f32(mat.ptr<float>(i) + j); // 加载4个元素
                        float32x4_t sqVec = vmulq_f32(vec, vec);            // 计算平方
                        totalSum = vaddq_f32(totalSum, sqVec);              // 累加平方和
                    }
                }

                totalSum = vpaddq_f32(totalSum, totalSum);       // 水平加法，得到总和的两个部分
                totalSum = vpaddq_f32(totalSum, totalSum);       // 再次水平加法，得到最终的总和
                sum3 = vget_lane_f32(vget_low_f32(totalSum), 0); // 提取总和的值
            }
#pragma omp section
            {
                float32x4_t totalSum = vdupq_n_f32(0.0f); // 初始化总和向量
                for (int i = row / 2; i < row; ++i)
                {
                    for (int j = col / 2; j < col; j += 4)
                    {
                        float32x4_t vec = vld1q_f32(mat.ptr<float>(i) + j); // 加载4个元素
                        float32x4_t sqVec = vmulq_f32(vec, vec);            // 计算平方
                        totalSum = vaddq_f32(totalSum, sqVec);              // 累加平方和
                    }
                }

                totalSum = vpaddq_f32(totalSum, totalSum);       // 水平加法，得到总和的两个部分
                totalSum = vpaddq_f32(totalSum, totalSum);       // 再次水平加法，得到最终的总和
                sum4 = vget_lane_f32(vget_low_f32(totalSum), 0); // 提取总和的值
            }
        }

        return sum1 + sum2 + sum3 + sum4; // 返回 Frobenius 范数
    }

    /**
     * @brief 计算边缘感知权重矩阵，根据输入图像的梯度信息，计算垂直和水平方向的权重矩阵，并将它们合并成一个矩阵
     * @return 无返回值，直接修改类成员变量 weightMatrix
     */
    void LimeEnhance::WeightCal()
    {
        cv::Mat dTv = dvMatrix * illumInitImage; // 计算垂直方向的梯度
        cv::Mat dTh = illumInitImage * dhMatrix; // 计算水平方向的梯度
        cv::Mat mv = 1 / (abs(dTv) + 1);         // 计算垂直方向的权重矩阵
        cv::Mat mh = 1 / (abs(dTh) + 1);         // 计算水平方向的权重矩阵
        cv::vconcat(mv, mh, weightMatrix);       // 将垂直和水平方向的权重矩阵合并成一个矩阵
    }

    /**
     * @brief 初始化光照图，构建拉普拉斯算子
     * @param src 输入图像
     * @return 无返回值，直接修改类成员变量 illumInitImage 和 laplacianMatrix
     */
    void LimeEnhance::InitIllum(const cv::Mat &src)
    {
        src.convertTo(normalImage, CV_32F, 1.0 / 255.0, 0); // 归一化

        rows = normalImage.rows;
        cols = normalImage.cols;

        illumInitImage = GetMax(normalImage);
        epsilon = FrobeniusNorm(illumInitImage) * 1e-3; // 设置收敛阈值基准

        dvMatrix = Dev(rows, 1);  // 构建垂直方向的差分矩阵
        dhMatrix = Dev(cols, -1); // 构建水平方向的差分矩阵

        // 构建拉普拉斯算子
        laplacianMatrix = cv::Mat(1, rows * cols, CV_32F, cv::Scalar(0.0)); // 初始化拉普拉斯算子矩阵
        laplacianMatrix.at<float>(0, 0) = 4.0;                              // 中心元素
        laplacianMatrix.at<float>(0, 1) = -1.0;                             // 右邻元素
        laplacianMatrix.at<float>(0, rows) = -1.0;                          // 下邻元素
        laplacianMatrix.at<float>(0, rows * cols - 1) = -1.0;               // 左邻元素
        laplacianMatrix.at<float>(0, rows * cols - rows) = -1.0;            // 上邻元素
    }

    /**
     * @brief 计算输入矩阵的 FFT，使用 Cooley-Tukey 算法实现，假设输入矩阵为单行矩阵，长度为 2 的幂次方
     * @param input 输入矩阵，假设为单行矩阵，长度为 2 的幂次方
     * @param output 输出矩阵，类型为复数（实部和虚部），长度与输入矩阵相同
     * @param opt 变换类型，1 表示正向 FFT，-1 表示逆向 FFT
     * @return 无返回值，直接修改输出矩阵 output
     */
    void LimeEnhance::FFT(const cv::Mat &input, cv::Mat &output, int opt)
    {
        if (input.empty())
        {
            output.release();
            return;
        }

        if (opt == 1)
        {
            if (input.type() == CV_32FC2)
            {
                cv::dft(input, output, 0);
            }
            else
            {
                cv::Mat realInput;
                input.convertTo(realInput, CV_32F);
                cv::dft(realInput, output, cv::DFT_COMPLEX_OUTPUT);
            }
        }
        else
        {
            cv::Mat complexInput;
            if (input.type() == CV_32FC2)
            {
                complexInput = input;
            }
            else
            {
                std::vector<cv::Mat> planes = {input, cv::Mat::zeros(input.size(), CV_32F)};
                cv::merge(planes, complexInput);
            }
            cv::dft(complexInput, output, cv::DFT_INVERSE | cv::DFT_SCALE);
        }
    }

    /**
     * @brief 位反转，FFT 蝶形运算需要
     * @param n 位数，即 FFT 的长度为 2^n
     * @param a 输入的整数，范围为 [0, 2^n - 1]
     * @return 返回 a 的位反转结果，即将 a 的二进制表示中的位顺序反转后的整数
     */
    int LimeEnhance::ReverseBin(int n, int a)
    {
        int result = 0;
        for (int i = 0; i < n; ++i)
        {
            if (a & (1 << i))
            {
                result |= 1 << (n - 1 - i);
            }
        }
        return result;
    }

}
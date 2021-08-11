#pragma once

#ifndef ENABLE_OPENCV_DNN

#include"AC.hpp"

namespace Anime4KCPP::CPU
{
    typedef float* ChanFP;
    typedef float* LineFP;
    typedef float* PixelFP;
    
    class CNNProcessor;
}

class Anime4KCPP::CPU::CNNProcessor
{
protected:
    void conv1To8(const cv::Mat& img, const float* kernels, const float* biases, cv::Mat& tmpMat);
    void conv8To8(const float* kernels, const float* biases, cv::Mat& tmpMat);
    void convTranspose8To1(cv::Mat& img, const float* kernels, cv::Mat& tmpMat);
};

#endif // !ENABLE_OPENCV_DNN
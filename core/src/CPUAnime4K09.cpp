#include"Parallel.hpp"
#include"FilterProcessor.hpp"
#include"CPUAnime4K09.hpp"

#define MAX3(a, b, c) std::max({a, b, c})
#define MIN3(a, b, c) std::min({a, b, c})

namespace Anime4KCPP::CPU::detail
{
    template<typename T, typename F>
    static void changEachPixel(cv::Mat& src, F&& callBack)
    {
        cv::Mat tmp;
        src.copyTo(tmp);

        const int h = src.rows, w = src.cols;
        const int channels = src.channels();
        const int jMAX = w * channels;
        const size_t step = src.step;

        Anime4KCPP::Utils::ParallelFor(0, h,
            [&](const int i) {
                T* lineData = reinterpret_cast<T*>(src.data + static_cast<size_t>(i) * step);
                T* tmpLineData = reinterpret_cast<T*>(tmp.data + static_cast<size_t>(i) * step);
                for (int j = 0; j < jMAX; j += channels)
                    callBack(i, j, tmpLineData + j, lineData);
            });

        src = tmp;
    }

    template<typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
    static constexpr T clampAndInvert(double v)
    {
        return std::numeric_limits<T>::max() -
            (v > std::numeric_limits<T>::max() ?
                std::numeric_limits<T>::max() :
                (std::numeric_limits<T>::min() > v ?
                    std::numeric_limits<T>::min() :
                    static_cast<T>(std::round(v))));
    }

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
    static constexpr T clampAndInvert(double v)
    {
        return static_cast<T>(1.0 - (v < 0.0 ? 0.0 : (1.0 < v ? 1.0 : v)));
    }

    template<typename T>
    static void getLightest(T* mc, const T* a, const T* b, const T* c, double strength) noexcept
    {
        constexpr double offset = std::is_floating_point<T>::value ? 0.0 : 0.5;
        for (int i = 0; i <= 3; i++)    //RGBA
            mc[i] = mc[i] + strength * (((a[i] + b[i] + c[i]) / 3.0) - mc[i]) + offset;
    }

    template<typename T>
    static void getAverage(T* mc, const T* a, const T* b, const T* c, double strength) noexcept
    {
        constexpr double offset = std::is_floating_point<T>::value ? 0.0 : 0.5;
        for (int i = 0; i <= 2; i++)    //RGB
            mc[i] = mc[i] + strength * (((a[i] + b[i] + c[i]) / 3.0) - mc[i]) + offset;
    }

    template<typename T>
    static void getGray(cv::Mat& img)
    {
        detail::changEachPixel<T>(img, [](const int i, const int j, T* pixel, T* curLine) {
            pixel[A] = pixel[R] * 0.299 + pixel[G] * 0.587 + pixel[B] * 0.114;
            });
    }

    template<typename T>
    static void pushColor(cv::Mat& img, double strength)
    {
        const int channels = img.channels();
        const size_t lineStep = img.step1();
        detail::changEachPixel<T>(img, [&](const int i, const int j, T* pixel, T* curLine) {
            const int jp = j < (img.cols - 1)* channels ? channels : 0;
            const int jn = j > channels ? -channels : 0;

            const T* const pLineData = i < img.rows - 1 ? curLine + lineStep : curLine;
            const T* const cLineData = curLine;
            const T* const nLineData = i > 0 ? curLine - lineStep : curLine;

            const T* const tl = nLineData + j + jn, * const tc = nLineData + j, * const tr = nLineData + j + jp;
            const T* const ml = cLineData + j + jn, * const mr = cLineData + j + jp;
            const T* const bl = pLineData + j + jn, * const bc = pLineData + j, * const br = pLineData + j + jp;
            T* const mc = pixel;

            T maxD, minL;

            //top and bottom
            maxD = MAX3(bl[A], bc[A], br[A]);
            minL = MIN3(tl[A], tc[A], tr[A]);
            if (minL > mc[A] && mc[A] > maxD)
                getLightest<T>(mc, tl, tc, tr, strength);
            else
            {
                maxD = MAX3(tl[A], tc[A], tr[A]);
                minL = MIN3(bl[A], bc[A], br[A]);
                if (minL > mc[A] && mc[A] > maxD)
                    getLightest<T>(mc, bl, bc, br, strength);
            }

            //sundiagonal
            maxD = MAX3(ml[A], mc[A], bc[A]);
            minL = MIN3(tc[A], tr[A], mr[A]);
            if (minL > maxD)
                getLightest<T>(mc, tc, tr, mr, strength);
            else
            {
                maxD = MAX3(tc[A], mc[A], mr[A]);
                minL = MIN3(ml[A], bl[A], bc[A]);
                if (minL > maxD)
                    getLightest<T>(mc, ml, bl, bc, strength);
            }

            //left and right
            maxD = MAX3(tl[A], ml[A], bl[A]);
            minL = MIN3(tr[A], mr[A], br[A]);
            if (minL > mc[A] && mc[A] > maxD)
                getLightest<T>(mc, tr, mr, br, strength);
            else
            {
                maxD = MAX3(tr[A], mr[A], br[A]);
                minL = MIN3(tl[A], ml[A], bl[A]);
                if (minL > mc[A] && mc[A] > maxD)
                    getLightest<T>(mc, tl, ml, bl, strength);
            }

            //diagonal
            maxD = MAX3(tc[A], mc[A], ml[A]);
            minL = MIN3(mr[A], br[A], bc[A]);
            if (minL > maxD)
                getLightest<T>(mc, mr, br, bc, strength);
            else
            {
                maxD = MAX3(bc[A], mc[A], mr[A]);
                minL = MIN3(ml[A], tl[A], tc[A]);
                if (minL > maxD)
                    getLightest<T>(mc, ml, tl, tc, strength);
            }
            });
    }

    template<typename T>
    static void getGradient(cv::Mat& img)
    {
        const int channels = img.channels();
        const size_t lineStep = img.step1();
        detail::changEachPixel<T>(img, [&](const int i, const int j, T* pixel, T* curLine) {
            const int jp = j < (img.cols - 1)* channels ? channels : 0;
            const int jn = j > channels ? -channels : 0;

            const T* const pLineData = i < img.rows - 1 ? curLine + lineStep : curLine;
            const T* const cLineData = curLine;
            const T* const nLineData = i > 0 ? curLine - lineStep : curLine;

            double gradX =
                (pLineData + j + jn)[A] + (pLineData + j)[A] + (pLineData + j)[A] + (pLineData + j + jp)[A] -
                (nLineData + j + jn)[A] - (nLineData + j)[A] - (nLineData + j)[A] - (nLineData + j + jp)[A];
            double gradY =
                (nLineData + j + jn)[A] + (cLineData + j + jn)[A] + (cLineData + j + jn)[A] + (pLineData + j + jn)[A] -
                (nLineData + j + jp)[A] - (cLineData + j + jp)[A] - (cLineData + j + jp)[A] - (pLineData + j + jp)[A];
            double grad = std::sqrt(gradX * gradX + gradY * gradY);

            pixel[A] = clampAndInvert<T>(grad);
            });
    }

    template<typename T>
    static void pushGradient(cv::Mat& img, double strength)
    {
        const int channels = img.channels();
        const size_t lineStep = img.step1();
        detail::changEachPixel<T>(img, [&](const int i, const int j, T* pixel, T* curLine) {
            const int jp = j < (img.cols - 1)* channels ? channels : 0;
            const int jn = j > channels ? -channels : 0;

            const T* const pLineData = i < img.rows - 1 ? curLine + lineStep : curLine;
            const T* const cLineData = curLine;
            const T* const nLineData = i > 0 ? curLine - lineStep : curLine;

            const T* const tl = nLineData + j + jn, * const tc = nLineData + j, * const tr = nLineData + j + jp;
            const T* const ml = cLineData + j + jn, * const mr = cLineData + j + jp;
            const T* const bl = pLineData + j + jn, * const bc = pLineData + j, * const br = pLineData + j + jp;
            T* const mc = pixel;

            T maxD, minL;

            //top and bottom
            maxD = MAX3(bl[A], bc[A], br[A]);
            minL = MIN3(tl[A], tc[A], tr[A]);
            if (minL > mc[A] && mc[A] > maxD)
                return getAverage<T>(mc, tl, tc, tr, strength);

            maxD = MAX3(tl[A], tc[A], tr[A]);
            minL = MIN3(bl[A], bc[A], br[A]);
            if (minL > mc[A] && mc[A] > maxD)
                return getAverage<T>(mc, bl, bc, br, strength);

            //sundiagonal
            maxD = MAX3(ml[A], mc[A], bc[A]);
            minL = MIN3(tc[A], tr[A], mr[A]);
            if (minL > maxD)
                return getAverage<T>(mc, tc, tr, mr, strength);

            maxD = MAX3(tc[A], mc[A], mr[A]);
            minL = MIN3(ml[A], bl[A], bc[A]);
            if (minL > maxD)
                return getAverage<T>(mc, ml, bl, bc, strength);

            //left and right
            maxD = MAX3(tl[A], ml[A], bl[A]);
            minL = MIN3(tr[A], mr[A], br[A]);
            if (minL > mc[A] && mc[A] > maxD)
                return getAverage<T>(mc, tr, mr, br, strength);

            maxD = MAX3(tr[A], mr[A], br[A]);
            minL = MIN3(tl[A], ml[A], bl[A]);
            if (minL > mc[A] && mc[A] > maxD)
                return getAverage<T>(mc, tl, ml, bl, strength);

            //diagonal
            maxD = MAX3(tc[A], mc[A], ml[A]);
            minL = MIN3(mr[A], br[A], bc[A]);
            if (minL > maxD)
                return getAverage<T>(mc, mr, br, bc, strength);

            maxD = MAX3(bc[A], mc[A], mr[A]);
            minL = MIN3(ml[A], tl[A], tc[A]);
            if (minL > maxD)
                return getAverage<T>(mc, ml, tl, tc, strength);
            });
    }

    template<typename T>
    static void processImpl(cv::Mat& src, const Parameters& param)
    {
        int pushColorCount = param.pushColorCount;

        for (int i = 0; i < param.passes; i++)
        {
            getGray<T>(src);
            if (param.strengthColor > 0.0 && (pushColorCount-- > 0))
                pushColor<T>(src, param.strengthColor);
            getGradient<T>(src);
            pushGradient<T>(src, param.strengthGradient);
        }
    }

    static void runKernel(cv::Mat& img, const Parameters& param)
    {
        switch (img.depth())
        {
        case CV_8U:
            processImpl<unsigned char>(img, param);
            break;
        case CV_16U:
            processImpl<unsigned short>(img, param);
            break;
        case CV_32F:
            processImpl<float>(img, param);
            break;
        default:
            throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
        }
    }
}

std::string Anime4KCPP::CPU::Anime4K09::getInfo()
{
    std::ostringstream oss;
    oss << AC::getInfo()
        << "----------------------------------------------" << std::endl
        << "Passes: " << param.passes << std::endl
        << "pushColorCount: " << param.pushColorCount << std::endl
        << "Zoom Factor: " << param.zoomFactor << std::endl
        << "Fast Mode: " << std::boolalpha << param.fastMode << std::endl
        << "Strength Color: " << param.strengthColor << std::endl
        << "Strength Gradient: " << param.strengthGradient << std::endl
        << "----------------------------------------------" << std::endl;
    return oss.str();
}

std::string Anime4KCPP::CPU::Anime4K09::getFiltersInfo()
{
    std::ostringstream oss;
    oss << AC::getFiltersInfo()
        << "----------------------------------------------" << std::endl
        << "Preprocessing filters list:" << std::endl
        << "----------------------------------------------" << std::endl;
    if (!param.preprocessing)
        oss << "Preprocessing disabled" << std::endl;
    else
    {
        std::vector<std::string>preFiltersString = FilterProcessor::filterToString(param.preFilters);
        if (preFiltersString.empty())
            oss << "Preprocessing disabled" << std::endl;
        else
            for (auto& filters : preFiltersString)
                oss << filters << std::endl;
    }

    oss << "----------------------------------------------" << std::endl
        << "Postprocessing filters list:" << std::endl
        << "----------------------------------------------" << std::endl;
    if (!param.postprocessing)
        oss << "Postprocessing disabled" << std::endl;
    else
    {
        std::vector<std::string>postFiltersString = FilterProcessor::filterToString(param.postFilters);
        if (postFiltersString.empty())
            oss << "Postprocessing disabled" << std::endl;
        else
            for (auto& filters : postFiltersString)
                oss << filters << std::endl;
    }

    return oss.str();
}

void Anime4KCPP::CPU::Anime4K09::processYUVImage()
{
    cv::Mat tmpImg;
    cv::merge(std::vector<cv::Mat>{ dstImg, orgU, orgV }, tmpImg);
    cv::cvtColor(tmpImg, tmpImg, cv::COLOR_YUV2BGR);

    if (param.zoomFactor == 2.0)
        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_LINEAR);
    else
        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    if (param.preprocessing)
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, dstImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)//PostProcessing
        FilterProcessor(dstImg, param.postFilters).process();

    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGR2YUV);
    std::vector<cv::Mat> yuv(3);
    cv::split(dstImg, yuv);
    dstImg = yuv[Y];
    dstU = yuv[U];
    dstV = yuv[V];
}

void Anime4KCPP::CPU::Anime4K09::processRGBImage()
{
    cv::Mat tmpImg;
    if (param.zoomFactor == 2.0)
        cv::resize(orgImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_LINEAR);
    else
        cv::resize(orgImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    if (param.preprocessing)// preprocessing
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, dstImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)// postprocessing
        FilterProcessor(dstImg, param.postFilters).process();
}

void Anime4KCPP::CPU::Anime4K09::processGrayscale()
{
    cv::Mat tmpImg;
    cv::cvtColor(orgImg, tmpImg, cv::COLOR_GRAY2BGR);

    if (param.zoomFactor == 2.0)
        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_LINEAR);
    else
        cv::resize(tmpImg, tmpImg, cv::Size(0, 0), param.zoomFactor, param.zoomFactor, cv::INTER_CUBIC);
    if (param.preprocessing)// preprocessing
        FilterProcessor(tmpImg, param.preFilters).process();
    cv::cvtColor(tmpImg, dstImg, cv::COLOR_BGR2BGRA);
    detail::runKernel(dstImg, param);
    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGRA2BGR);
    if (param.postprocessing)// postprocessing
        FilterProcessor(dstImg, param.postFilters).process();

    cv::cvtColor(dstImg, dstImg, cv::COLOR_BGR2GRAY);
}

Anime4KCPP::Processor::Type Anime4KCPP::CPU::Anime4K09::getProcessorType() noexcept
{
    return Processor::Type::CPU_Anime4K09;
}

std::string Anime4KCPP::CPU::Anime4K09::getProcessorInfo()
{
    std::ostringstream oss;
    oss << "Processor type: " << getProcessorType();
    return oss.str();
}
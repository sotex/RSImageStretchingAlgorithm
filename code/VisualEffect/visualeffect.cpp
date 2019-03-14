#include "visualeffect.hpp"

#include <gdal.h>
#include <cpl_auto_close.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cfloat>

#ifndef PI
#define PI 3.141592653589793
#endif

namespace std
{
template<typename T>
inline T clamp(T minval, T val, T maxval)
{
    return (minval > val)? minval : (maxval < val)? maxval: val;
}

// 标准正态分布函数
// 期望值μ=0，即曲线图象对称轴为Y轴，标准差σ=1条件下的正态分布
double stdnormpdf(double x)
{
  return( (1.0 / sqrt(2.0 * PI))*exp(-0.5*x*x));
}

// 正态分布概率密度函数
// mu    数学期望(平均值)
// sigma 标准差
double normpdf(double x, double mu, double sigma)
{
  return( (1.0 /(sigma *  sqrt(2.0 * PI))) * exp( -1 * (x-mu)*(x-mu)/(2*sigma*sigma)));
}

}


VisualEffect::VisualEffect(QWidget *parent)
    : QWidget(parent)
{
    GDALAllRegister();

    QLineEdit* pLned = new QLineEdit(this);
    QPushButton* pBtnSelFile = new QPushButton(QStringLiteral("选择影像文件"),this);
    QPushButton* pBtnUpdate = new QPushButton(QStringLiteral("刷新显示"),this);

    QHBoxLayout* pHLayout1 = new QHBoxLayout;
    pHLayout1->addWidget(pLned);
    pHLayout1->addWidget(pBtnSelFile);
    pHLayout1->addWidget(pBtnUpdate);


    QLabel* pLab = new QLabel(QStringLiteral("选择RGB波段序号"),this);
    QSpinBox* pSBoxR = new QSpinBox(this);
    QSpinBox* pSBoxG = new QSpinBox(this);
    QSpinBox* pSBoxB = new QSpinBox(this);

    QComboBox* pCBox = new QComboBox(this);
    pCBox->addItems({QStringLiteral("线性拉伸"),
                     QStringLiteral("2%线性拉伸"),
                     QStringLiteral("直方图均衡化"),
                     QStringLiteral("直方图规定化(正态分布)")});

    QHBoxLayout* pHLayout2 = new QHBoxLayout;
    pHLayout2->addWidget(pLab);
    pHLayout2->addWidget(pSBoxR);
    pHLayout2->addWidget(pSBoxG);
    pHLayout2->addWidget(pSBoxB);
    pHLayout2->addStretch();
    pHLayout2->addWidget(pCBox);


    QLabel* pLab1 = new QLabel(QStringLiteral("显示原图"),this);
    QLabel* pLab2 = new QLabel(QStringLiteral("显示处理图"),this);

    QSplitter* pSpt = new QSplitter;
    pSpt->addWidget(pLab1);
    pSpt->addWidget(pLab2);

    QVBoxLayout* pVLayout = new QVBoxLayout(this);
    pVLayout->addItem(pHLayout1);
    pVLayout->addItem(pHLayout2);
    pVLayout->addWidget(pSpt);

    this->resize(640,480);

    // 选择文件按钮单击处理
    connect(pBtnSelFile,&QPushButton::clicked,[=](){
        QString filename = QFileDialog::getOpenFileName(this,
                                                        QStringLiteral("打开影像文件"),
                                                        QStringLiteral("."),
                                                        QStringLiteral("Images (*.tif *.tiff *.img *.jpg *.jpeg)"));
        if(filename.isEmpty()){
            return;
        }
        pLned->setText(filename);
        // 打开影像
        QByteArray u8filename = filename.toUtf8();
        std::string u8fnstr(u8filename.data(),static_cast<size_t>(u8filename.size()));
        GDALDatasetH hDset = GDALOpen(u8fnstr.c_str(),GA_ReadOnly);
        CPL_AUTO_CLOSE_WARP(hDset,GDALClose);
        if(hDset == nullptr){ return; }

        // 获取波段信息
        int nBands = GDALGetRasterCount(hDset);
        if(nBands==0){ return; }
        pSBoxR->setRange(1,nBands); pSBoxR->setValue(1);
        pSBoxG->setRange(1,nBands); pSBoxG->setValue(std::min(2,nBands));
        pSBoxB->setRange(1,nBands); pSBoxB->setValue(std::min(3,nBands));

        // 获取影像大小信息
        int nXSize = GDALGetRasterXSize(hDset);
        int nYSize = GDALGetRasterYSize(hDset);

        // 读取影像数据(全部读取为double类型)
        int nTypeBytes = GDALGetDataTypeSizeBytes(GDT_Float64);
        QByteArray buffer(nTypeBytes*nXSize*nYSize*nBands,'\0');
        double* pBuffer = reinterpret_cast<double*>(buffer.data());
        CPLErr err = GDALDatasetRasterIO(hDset,GF_Read,
                                         0,0,nXSize,nYSize,
                                         pBuffer,nXSize,nYSize,GDT_Float64,
                                         nBands,nullptr,
                                         nTypeBytes,nTypeBytes*nXSize,nTypeBytes*nXSize*nYSize);
        if(err != CE_None){
            qDebug()<<CPLGetLastErrorMsg();
            return;
        }

        this->setProperty("nBands",QVariant(nBands));
        this->setProperty("nXSize",QVariant(nXSize));
        this->setProperty("nYSize",QVariant(nYSize));
        this->setProperty("buffer",QVariant(buffer));
    });

    // 刷新显示按钮单击处理
    connect(pBtnUpdate,&QPushButton::clicked,[=](){
        QString Alg = pCBox->currentText();
        const int nBands = this->property("nBands").toInt();
        const int nXSize = this->property("nXSize").toInt();
        const int nYSize = this->property("nYSize").toInt();
        QByteArray buffer = this->property("buffer").toByteArray();
        if(nBands == 0 || nXSize == 0 || nYSize == 0
                || buffer.isEmpty()){
            return;
        }
        const double* pBuffer = reinterpret_cast<const double*>(buffer.data());
        const int iRBand = pSBoxR->value();
        const int iGBand = pSBoxG->value();
        const int iBBand = pSBoxB->value();
        const double* pRBuffer = pBuffer + (nXSize*nYSize*(iRBand-1));
        const double* pGBuffer = pBuffer + (nXSize*nYSize*(iGBand-1));
        const double* pBBuffer = pBuffer + (nXSize*nYSize*(iBBand-1));

        {
            QImage image(nXSize,nYSize,QImage::Format_RGB888);
            uchar* pOut = image.bits();
            for(int y = 0;y<nYSize;++y){
                for(int x=0; x<nXSize;++x){
                    uchar* pPixel = pOut + (y*nXSize+x)*3;
                    pPixel[0] = static_cast<uchar>(pRBuffer[y*nXSize+x]);
                    pPixel[1] = static_cast<uchar>(pGBuffer[y*nXSize+x]);
                    pPixel[2] = static_cast<uchar>(pBBuffer[y*nXSize+x]);
                    // limage.setPixelColor(x,y,QColor(r,g,b));
                }
            }
            pLab1->setPixmap(QPixmap::fromImage(image.scaledToWidth(640)));
        }

        {
            // 统计三个波段的最大最小值
            // auto mimmaxpair = std::minmax_element(pRBuffer,pRBuffer+(nXSize*nYSize));
            // const double dfRMin = *mimmaxpair.first; const double dfRMax = *mimmaxpair.second;
            // mimmaxpair = std::minmax_element(pGBuffer,pGBuffer+(nXSize*nYSize));
            // const double dfGMin = *mimmaxpair.first; const double dfGMax = *mimmaxpair.second;
            // mimmaxpair = std::minmax_element(pBBuffer,pBBuffer+(nXSize*nYSize));
            // const double dfBMin = *mimmaxpair.first; const double dfBMax = *mimmaxpair.second;

            // 不包括无效值0 统计最大最小值
            double dfRMin = DBL_MAX,dfGMin = DBL_MAX,dfBMin = DBL_MAX;
            double dfRMax = -DBL_MAX,dfGMax = -DBL_MAX,dfBMax = -DBL_MAX;
            // 统计有效像素个数(至少有一个波段不为0)，累计值
            double dfRSum = 0.0, dfGSum = 0.0, dfBSum = 0.0;
            size_t nPixelCount = 0;
            for(int i=0;i<nXSize*nYSize;++i){
                double v = pRBuffer[i];
                double isValid = v; dfRSum += v;
                if(v != 0.0){ dfRMin = std::min(dfRMin,v); dfRMax = std::max(dfRMax,v); }

                v = pGBuffer[i];
                dfGSum += v; isValid = (v != 0? v:isValid);
                if(v != 0.0){ dfGMin = std::min(dfRMin,v); dfGMax = std::max(dfRMax,v); }

                v = pBBuffer[i];
                dfBSum += v; isValid = (v != 0? v:isValid);
                if(v != 0.0){ dfBMin = std::min(dfRMin,v); dfBMax = std::max(dfRMax,v); }
                // 三个都为0，才认为像素无效
                if(isValid){ nPixelCount += 1; }

                // double valueR = pRBuffer[i];
                // double valueG = pGBuffer[i];
                // double valueB = pBBuffer[i];
                // 三个都为0，才认为像素无效
                // if(valueR == 0.0 && valueG == 0.0 && valueB == 0.0){
                //     continue;
                // }
                // nPixelCount += 1;
                // dfRSum += valueR;
                // dfGSum += valueG;
                // dfBSum += valueB;
                // dfRMin = std::min(dfRMin,valueR); dfRMax = std::max(dfRMax,valueR);
                // dfGMin = std::min(dfGMin,valueG); dfGMax = std::max(dfGMax,valueG);
                // dfBMin = std::min(dfBMin,valueB); dfBMax = std::max(dfBMax,valueB);
            }

            QImage image(nXSize,nYSize,QImage::Format_RGBA8888);
            uchar* pOut = image.bits();

            if(Alg == QStringLiteral("线性拉伸")) {
                for(int y = 0;y<nYSize;++y){
                    for(int x=0; x<nXSize;++x){
                        uchar* pPixel = pOut + (y*nXSize+x)*4;

                        double valueR = pRBuffer[y*nXSize+x];
                        double valueG = pGBuffer[y*nXSize+x];
                        double valueB = pBBuffer[y*nXSize+x];
                        // 判断是否是无效像素
                        if(valueR == 0.0 && valueG == 0.0 && valueB == 0.0){
                            pPixel[3] = 0.0; continue;
                        }

                        // 0-255线性映射到[最小值,最大值]。
                        pPixel[0] = static_cast<uchar>((valueR -dfRMin)/(dfRMax-dfRMin) *255.0);
                        pPixel[1] = static_cast<uchar>((valueG -dfGMin)/(dfGMax-dfGMin) *255.0);
                        pPixel[2] = static_cast<uchar>((valueB -dfBMin)/(dfBMax-dfBMin) *255.0);
                        pPixel[3] = 255;
                        // limage.setPixelColor(x,y,QColor(r,g,b));
                    }
                }
            }
            else if(Alg == QStringLiteral("2%线性拉伸")) {
                double diff = dfRMax-dfRMin;
                dfRMin += diff * 0.02; dfRMax -= diff * 0.02;
                diff = dfGMax-dfGMin;
                dfGMin += diff * 0.02; dfGMax -= diff * 0.02;
                diff = dfBMax-dfBMin;
                dfBMin += diff * 0.02; dfBMax -= diff * 0.02;

                for(int y = 0;y<nYSize;++y){
                    for(int x=0; x<nXSize;++x){
                        uchar* pPixel = pOut + (y*nXSize+x)*4;

                        double valueR = pRBuffer[y*nXSize+x];
                        double valueG = pGBuffer[y*nXSize+x];
                        double valueB = pBBuffer[y*nXSize+x];
                        // 判断是否是无效像素
                        if(valueR == 0.0 && valueG == 0.0 && valueB == 0.0){
                            pPixel[3] = 0.0; continue;
                        }
                        // 相当于0-255线性映射到[最小值+2%,最大值-%2]
                        // 这里应该考虑像素值与最小值相减为负数的情况，已经映射值超过255的情况
                        pPixel[0] = static_cast<uchar>(std::clamp(0.0, ((valueR -dfRMin)/(dfRMax-dfRMin) *255.0),255.0));
                        pPixel[1] = static_cast<uchar>(std::clamp(0.0, ((valueG -dfGMin)/(dfGMax-dfGMin) *255.0),255.0));
                        pPixel[2] = static_cast<uchar>(std::clamp(0.0, ((valueB -dfBMin)/(dfBMax-dfBMin) *255.0),255.0));
                        pPixel[3] = 255;
                        // limage.setPixelColor(x,y,QColor(r,g,b));
                    }
                }
            }
            else if(Alg == QStringLiteral("直方图均衡化")){
                // 计算直方图，将RGB各个波段在最大最小值之间分为1024个灰度级进行统计
                std::vector<double> histogramR(1024,0);
                std::vector<double> histogramG(1024,0);
                std::vector<double> histogramB(1024,0);
                for(int i=0;i<nXSize*nYSize;++i){
                    double valueR = pRBuffer[i];
                    double valueG = pGBuffer[i];
                    double valueB = pBBuffer[i];
                    if(valueR !=0.0 || valueG != 0.0 || valueB != 0.0){
                        histogramR[std::clamp<int>(0,((valueR-dfRMin)*1024/(dfRMax-dfRMin)),1023)] += 1;
                        histogramG[std::clamp<int>(0,((valueG-dfGMin)*1024/(dfGMax-dfGMin)),1023)] += 1;
                        histogramB[std::clamp<int>(0,((valueB-dfBMin)*1024/(dfGMax-dfBMin)),1023)] += 1;
                    }
                }
                // 下面两个计算可以合二为一
                // 计算为概率值
                double factor = 1.0/nPixelCount;
                for(size_t i=0;i<1024;++i){
                    histogramR[i] *= factor;
                    histogramG[i] *= factor;
                    histogramB[i] *= factor;
                }
                // 计算颜色表(计算1024个灰度级别，按出现概率映射的0-255的值)
                // 颜色值重新映射后再计算直方图，应该每个像素值出现的概率是相同的
                double cdfR = 0, cdfB = 0, cdfG = 0;
                std::vector<uchar> cltR(1024,0);    // color lookup table
                std::vector<uchar> cltG(1024,0);
                std::vector<uchar> cltB(1024,0);
                for(size_t i=0;i<1024;++i){
                    cdfR += histogramR[i]; cltR[i] = static_cast<uchar>(cdfR*255);
                    cdfG += histogramG[i]; cltG[i] = static_cast<uchar>(cdfG*255);
                    cdfB += histogramB[i]; cltB[i] = static_cast<uchar>(cdfB*255);
                }
                // 输出RGBA图像
                for(int y = 0;y<nYSize;++y){
                    for(int x=0; x<nXSize;++x){
                        uchar* pPixel = pOut + (y*nXSize+x)*4;
                        double valueR = pRBuffer[y*nXSize+x];
                        double valueG = pGBuffer[y*nXSize+x];
                        double valueB = pBBuffer[y*nXSize+x];
                        // 判断是否是无效像素
                        if(valueR == 0.0 && valueG == 0.0 && valueB == 0.0){
                            pPixel[3] = 0.0; continue;
                        }
                        pPixel[0] = cltR[std::clamp<int>(0,((valueR-dfRMin)*1024/(dfRMax-dfRMin)),1023)];
                        pPixel[1] = cltG[std::clamp<int>(0,((valueG-dfGMin)*1024/(dfGMax-dfGMin)),1023)];
                        pPixel[2] = cltB[std::clamp<int>(0,((valueB-dfBMin)*1024/(dfGMax-dfBMin)),1023)];
                        pPixel[3] = 255;
                    }
                }
            }
            else if(Alg == QStringLiteral("直方图规定化(正态分布)")){
                // 直方图规定化即以一个给定的直方图来替代原图的直方图
                // 这里直接用正态分布概率密度曲线（高斯曲线）来替代
                // 计算三个波段的平均值和标准差
                double dfRMean = dfRSum / nPixelCount;
                double dfGMean = dfGSum / nPixelCount;
                double dfBMean = dfBSum / nPixelCount;
                double dfRStddev = 0.0, dfGStddev = 0.0, dfBStddev = 0.0;
                for(int i=0;i<nXSize*nYSize;++i){
                    double valueR = pRBuffer[i];
                    double valueG = pGBuffer[i];
                    double valueB = pBBuffer[i];
                    if(valueR !=0.0 || valueG != 0.0 || valueB != 0.0){
                       dfRStddev += (valueR - dfRMean)*(valueR - dfRMean);
                       dfGStddev += (valueG - dfGMean)*(valueG - dfGMean);
                       dfBStddev += (valueB - dfBMean)*(valueB - dfBMean);
                    }
                }
                dfRStddev = sqrt(dfRStddev/nPixelCount);
                dfGStddev = sqrt(dfGStddev/nPixelCount);
                dfBStddev = sqrt(dfBStddev/nPixelCount);
                // 将各个波段的最大最小值限制在均值左右各3倍标准差的范围内
                dfRMin = dfRMean - dfRStddev*2.5; dfRMax = dfRMean + dfRStddev*2.5;
                dfGMin = dfGMean - dfGStddev*2.5; dfGMax = dfGMean + dfGStddev*2.5;
                dfBMin = dfBMean - dfBStddev*2.5; dfBMax = dfBMean + dfBStddev*2.5;
                double dfRSpace = (dfRMax - dfRMin) / 1023.0;
                double dfGSpace = (dfGMax - dfGMin) / 1023.0;
                double dfBSpace = (dfBMax - dfBMin) / 1023.0;

                // 根据三个波段的平均值和标准差生成直方图(概率值)
                std::vector<double> histogramR(1024,0);
                std::vector<double> histogramG(1024,0);
                std::vector<double> histogramB(1024,0);
                for(size_t i=0;i<1024;++i){
                    histogramR[i] = std::normpdf(dfRMin + i*dfRSpace, dfRMean, dfRStddev);
                    histogramG[i] = std::normpdf(dfGMin + i*dfGSpace, dfGMean, dfGStddev);
                    histogramB[i] = std::normpdf(dfBMin + i*dfBSpace, dfBMean, dfBStddev);
                }
                // 计算颜色表(计算1024个灰度级别，按出现概率映射的0-255的值)
                // 颜色值重新映射后再计算直方图，应该是符合正态分布概率密度曲线的
                // 需要计算归化概率累计值到[0,255]区间的 比率系数=255/概率值总和
                double sumHistR = std::accumulate(histogramR.begin(),histogramR.end(),0.0);
                double sumHistG = std::accumulate(histogramG.begin(),histogramG.end(),0.0);
                double sumHistB = std::accumulate(histogramB.begin(),histogramB.end(),0.0);
                double cdfR = 0, cdfB = 0, cdfG = 0;
                std::vector<uchar> cltR(1024,0);    // color lookup table
                std::vector<uchar> cltG(1024,0);
                std::vector<uchar> cltB(1024,0);
                for(size_t i=0;i<1024;++i){
                    cdfR += histogramR[i]; cltR[i] = static_cast<uchar>(cdfR*255/sumHistR);
                    cdfG += histogramG[i]; cltG[i] = static_cast<uchar>(cdfG*255/sumHistG);
                    cdfB += histogramB[i]; cltB[i] = static_cast<uchar>(cdfB*255/sumHistB);
                }
                // 输出RGBA图像
                for(int y = 0;y<nYSize;++y){
                    for(int x=0; x<nXSize;++x){
                        uchar* pPixel = pOut + (y*nXSize+x)*4;
                        double valueR = pRBuffer[y*nXSize+x];
                        double valueG = pGBuffer[y*nXSize+x];
                        double valueB = pBBuffer[y*nXSize+x];
                        // 判断是否是无效像素
                        if(valueR == 0.0 && valueG == 0.0 && valueB == 0.0){
                            pPixel[3] = 0.0; continue;
                        }
                        pPixel[0] = cltR[std::clamp<int>(0,((valueR-dfRMin)/dfRSpace),1023)];
                        pPixel[1] = cltG[std::clamp<int>(0,((valueG-dfGMin)/dfGSpace),1023)];
                        pPixel[2] = cltB[std::clamp<int>(0,((valueB-dfBMin)/dfBSpace),1023)];
                        pPixel[3] = 255;
                    }
                }
            }
            pLab2->setPixmap(QPixmap::fromImage(image.scaledToWidth(640)));
        }
    });
}

VisualEffect::~VisualEffect()
{
}

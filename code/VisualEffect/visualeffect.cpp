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
    pCBox->addItems({QStringLiteral("线性拉伸"),QStringLiteral("2%线性拉伸")});

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
                                                        QStringLiteral("Images (*.tif *.tiff *.img)"));
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
        pSBoxR->setRange(1,nBands);
        pSBoxG->setRange(1,nBands);
        pSBoxB->setRange(1,nBands);

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
            for(int i=0;i<nXSize*nYSize;++i){
                double v = pRBuffer[i];
                if(v != 0.0){ dfRMin = std::min(dfRMin,v); dfRMax = std::max(dfRMax,v); }
                v = pGBuffer[i];
                if(v != 0.0){ dfGMin = std::min(dfRMin,v); dfGMax = std::max(dfRMax,v); }
                v = pBBuffer[i];
                if(v != 0.0){ dfBMin = std::min(dfRMin,v); dfBMax = std::max(dfRMax,v); }
            }

            QImage image(nXSize,nYSize,QImage::Format_RGBA8888);
            uchar* pOut = image.bits();
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

                    // 这里应该考虑无效值0与最小值相减为负数的情况
                    pPixel[0] = static_cast<uchar>((valueR -dfRMin)/(dfRMax-dfRMin) *255.0);
                    pPixel[1] = static_cast<uchar>((valueG -dfGMin)/(dfGMax-dfGMin) *255.0);
                    pPixel[2] = static_cast<uchar>((valueB -dfBMin)/(dfBMax-dfBMin) *255.0);
                    pPixel[3] = 255;
                    // limage.setPixelColor(x,y,QColor(r,g,b));
                }
            }
            pLab2->setPixmap(QPixmap::fromImage(image.scaledToWidth(640)));
        }
    });
}

VisualEffect::~VisualEffect()
{
}

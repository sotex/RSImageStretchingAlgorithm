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


VisualEffect::VisualEffect(QWidget *parent)
    : QWidget(parent)
{

    QLineEdit* pLned = new QLineEdit(this);
    QPushButton* pBtn = new QPushButton(QStringLiteral("选择影像文件"),this);
    QComboBox* pCBox = new QComboBox(this);
    pCBox->addItems({QStringLiteral("线性拉伸"),QStringLiteral("2%线性拉伸")});

    QHBoxLayout* pHLayout1 = new QHBoxLayout;
    pHLayout1->addWidget(pLned);
    pHLayout1->addWidget(pBtn);
    pHLayout1->addWidget(pCBox);


    QLabel* pLab = new QLabel(QStringLiteral("选择RGB波段序号"),this);
    QSpinBox* pSBoxR = new QSpinBox(this);
    QSpinBox* pSBoxG = new QSpinBox(this);
    QSpinBox* pSBoxB = new QSpinBox(this);

    QHBoxLayout* pHLayout2 = new QHBoxLayout;
    pHLayout2->addWidget(pLab);
    pHLayout2->addWidget(pSBoxR);
    pHLayout2->addWidget(pSBoxG);
    pHLayout2->addWidget(pSBoxB);


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

    connect(pBtn,&QPushButton::clicked,[=](){
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
        GDALDatasetH hDset = GDALOpen(std::string(u8filename.data(),int(u8filename.size())).c_str(),GA_ReadOnly);
        CPL_AUTO_CLOSE_WARP(hDset,GDALClose);
        if(hDset == 0){ return; }

        // 获取波段信息
        int nBands = GDALGetRasterCount(hDset);
        if(nBands==0){ return; }
        pSBoxR->setRange(1,nBands);
        pSBoxG->setRange(1,nBands);
        pSBoxB->setRange(1,nBands);
        this->setProperty("nBands",QVariant(nBands));

        // 获取影像大小信息
        int nXSize = GDALGetRasterXSize(hDset);
        int nYSize = GDALGetRasterYSize(hDset);
        this->setProperty("nXSize",QVariant(nXSize));
        this->setProperty("nYSize",QVariant(nYSize));

        // 读取影像数据(全部读取为double类型)
        int nTypeBytes = GDALGetDataTypeSizeBytes(GDT_Float64);
        QByteArray buffer(nTypeBytes*nXSize*nYSize*nBands,'\0');
        CPLErr err = GDALDatasetRasterIO(hDset,GF_Read,
                                         0,0,nXSize,nYSize,
                                         reinterpret_cast<void*>(buffer.data()),nXSize,nYSize,GDT_Float64,
                                         nBands,0,
                                         nTypeBytes,nTypeBytes*nXSize,nTypeBytes*nXSize*nYSize);
        if(err != CE_None){
            qDebug()<<CPLGetLastErrorMsg();
            return;
        }
        this->setProperty("buffer",QVariant(buffer));

    });

}

VisualEffect::~VisualEffect()
{
}

#ifndef VISUALEFFECT_HPP
#define VISUALEFFECT_HPP

#include <QWidget>

class VisualEffect : public QWidget
{
    Q_OBJECT

public:
    VisualEffect(QWidget *parent = 0);
    ~VisualEffect();
};

#endif // VISUALEFFECT_HPP

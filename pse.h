#ifndef PSE_H
#define PSE_H

#include <QDialog>

QT_USE_NAMESPACE

QT_BEGIN_NAMESPACE

class psgPoint
{
public:
    QString Name;
    int     TimePoint;
    bool    DigitalO[16];
    float   DCbias[16];
    bool    Loop;
    QString LoopName;
    int     LoopCount;

    explicit psgPoint();
    void operator=(const psgPoint &P )
    {
       Name = P.Name;
       TimePoint = P.TimePoint;
       Loop = P.Loop;
       LoopName = P.LoopName;
       LoopCount = P.LoopCount;
       for(int i=0;i<16;i++) { DigitalO[i]=P.DigitalO[i]; DCbias[i]=P.DCbias[i];}
    }

};
QDataStream &operator<<(QDataStream &out, const psgPoint &point);
QDataStream &operator>>(QDataStream &in, psgPoint &point);

namespace Ui {
class pseDialog;
}

QT_END_NAMESPACE

class pseDialog : public QDialog
{
    Q_OBJECT

public:
    //    explicit pseDialog(std::vector<psgPoint*> psg, QWidget *parent = 0);
    explicit pseDialog(QList<psgPoint*> *psg, QWidget *parent = 0);
    void UpdateDialog(psgPoint *point);
    ~pseDialog();

private slots:
    void on_DIO_checked();
    void on_pbNext_pressed();
    void on_pbPrevious_pressed();
    void on_DCBIAS_edited();
    void on_pbInsert_pressed();
    void on_pbDelete_pressed();
    void on_leName_textChanged(const QString &arg1);
    void on_leClocks_textChanged(const QString &arg1);
    void on_leCycles_textChanged(const QString &arg1);
    void on_chkLoop_clicked(bool checked);
    void on_comboLoop_currentIndexChanged(const QString &arg1);

private:
    Ui::pseDialog *ui;
    int CurrentIndex;
    psgPoint *activePoint;
    QList<psgPoint*> *p;
};

#endif // PSE_H

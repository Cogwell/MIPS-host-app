#ifndef MIPS_H
#define MIPS_H

#include <QMainWindow>
#include <QtCore/QtGlobal>
#include <QtSerialPort/QSerialPort>
#include <QLineEdit>
#include <QTimer>

namespace Ui {
class MIPS;
}

class Console;
class SettingsDialog;

class MIPS : public QMainWindow
{
    Q_OBJECT

public:
    explicit MIPS(QWidget *parent = 0);
    ~MIPS();
    void openSerialPort();
    void closeSerialPort();
    virtual void resizeEvent(QResizeEvent* event);
    virtual void mousePressEvent(QMouseEvent * event);
    QString SendMessage(QString);
    void SendCommand(QString message);

private slots:
    void MIPSconnect(void);
    void MIPSdisconnect(void);
    void tabSelected();
    void writeData(const QByteArray &data);
    void readData2Console(void);
    void readData2RingBuffer(void);
    void UpdateDCbias(void);
    void handleError(QSerialPort::SerialPortError error);
    void DCbiasUpdated(void);
    void pollLoop(void);
    void UpdateDIO(void);
    void DOUpdated(void);
    void TrigHigh(void);
    void TrigLow(void);
    void TrigPulse(void);
    void DCbiasPower(void);
    void UpdateRFdriver(void);

private:
    Ui::MIPS *ui;
    Console *console;
    SettingsDialog *settings;
    QSerialPort *serial;
    QTimer *pollTimer;
};

#endif // MIPS_H

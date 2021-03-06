#ifndef MIPS_H
#define MIPS_H

#include <QMainWindow>
#include <QtCore/QtGlobal>
#include <QtSerialPort/QSerialPort>
#include <QLineEdit>
#include <QTimer>
#include <QProcess>
//#include <qtcpsocket.h>
#include <QtNetwork/QTcpSocket>


namespace Ui {
class MIPS;
}

class Console;
class SettingsDialog;
class pseDialog;
class psgPoint;

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
    int Referenced(QList<psgPoint*> P, int i);
    QString BuildTableCommand(QList<psgPoint*> P);
    void UpdatePSG(void);

private slots:
    void setWidgets(QWidget*, QWidget*);
    void MIPSconnect(void);
    void MIPSsetup(void);
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
    void loadSettings(void);
    void saveSettings(void);
    void DisplayAboutMessage(void);
    void programMIPS(void);
    void executeProgrammerCommand(QString cmd);
    void setBootloaderBootBit(void);
    void saveMIPSfirmware(void);
    void readProcessOutput(void);
    void connected(void);
    void disconnected(void);
    // These slots automatically connect due to naming convention
    void on_pbDownload_pressed(void);
    void on_pbViewTable_pressed();
    void on_pbLoadFromFile_pressed();
    void on_pbCreateNew_pressed();
    void on_pbSaveToFile_pressed();
    void on_pbEditCurrent_pressed();
    void on_leSequenceNumber_textEdited(const QString &arg1);
    void on_chkAutoAdvance_clicked(bool checked);
    void on_pbTrigger_pressed();
    void on_leSRFFRQ_editingFinished();
    void on_leSRFDRV_editingFinished();
    void on_pbRead_pressed();
    void on_pbWrite_pressed();

private:
    Ui::MIPS *ui;
    Console *console;
    SettingsDialog *settings;
    pseDialog *pse;
    QSerialPort *serial;
    QTimer *pollTimer;
    QList<psgPoint*> psg;
    QProcess process;
    QString  appPath;
    QTcpSocket client;
    bool client_connected;
};

#endif // MIPS_H

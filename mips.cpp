//
//    QTime timer;
//    timer.start();
//    while(timer.elapsed() < 1000) QApplication::processEvents();
//    ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "Done");
//
#include "mips.h"
#include "ui_mips.h"
#include "console.h"
#include "settingsdialog.h"
#include "ringbuffer.h"

#include <QMessageBox>
#include <QtSerialPort/QSerialPort>
#include <QMessageBox>
#include <QTime>
#include <QThread>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QCursor>
#include <QDebug>

RingBuffer rb;

MIPS::MIPS(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MIPS)
{
    ui->setupUi(this);

    pollTimer = new QTimer;
    console = new Console(ui->Terminal);
    console->setEnabled(false);
    serial = new QSerialPort(this);
    settings = new SettingsDialog;

    connect(ui->pbConfigure, SIGNAL(pressed()), settings, SLOT(show()));
    connect(ui->tabMIPS,SIGNAL(currentChanged(int)),this,SLOT(tabSelected()));
    connect(ui->pbConnect,SIGNAL(pressed()),this,SLOT(MIPSconnect()));
    connect(ui->pbDisconnect,SIGNAL(pressed()),this,SLOT(MIPSdisconnect()));
    connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this,SLOT(handleError(QSerialPort::SerialPortError)));
    connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollLoop()));
    // DCbias page setup
    QObjectList widgetList = ui->gbDCbias1->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("leSDCB"))
       {
            ((QLineEdit *)w)->setValidator(new QDoubleValidator);
            connect(((QLineEdit *)w),SIGNAL(editingFinished()),this,SLOT(DCbiasUpdated()));
       }
    }
    widgetList = ui->gbDCbias2->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("leSDCB"))
       {
            ((QLineEdit *)w)->setValidator(new QDoubleValidator);
            connect(((QLineEdit *)w),SIGNAL(editingFinished()),this,SLOT(DCbiasUpdated()));
       }
    }
    connect(ui->pbDCbiasUpdate,SIGNAL(pressed()),this,SLOT(UpdateDCbias()));
    connect(ui->chkPowerEnable,SIGNAL(toggled(bool)),this,SLOT(DCbiasPower()));
    // DIO page setup
    widgetList = ui->gbDigitalOut->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("chk"))
       {
            connect(((QCheckBox *)w),SIGNAL(stateChanged(int)),this,SLOT(DOUpdated()));
       }
    }
    connect(ui->pbDIOupdate,SIGNAL(pressed()),this,SLOT(UpdateDIO()));
    connect(ui->pbTrigHigh,SIGNAL(pressed()),this,SLOT(TrigHigh()));
    connect(ui->pbTrigLow,SIGNAL(pressed()),this,SLOT(TrigLow()));
    connect(ui->pbTrigPulse,SIGNAL(pressed()),this,SLOT(TrigPulse()));
    // RF driver page setup
    connect(ui->pbUpdateRF,SIGNAL(pressed()),this,SLOT(UpdateRFdriver()));
    ui->leSRFFRQ->setValidator(new QIntValidator);
    ui->leSRFDRV->setValidator(new QDoubleValidator);
    // Sets the polling loop interval and starts the timer
    pollTimer->start(1000);
}

MIPS::~MIPS()
{
    delete settings;
    delete ui;
}

void MIPS::pollLoop(void)
{

}

void MIPS::DCbiasUpdated(void)
{
   QObject* obj = sender();
   QString res;

   res = obj->objectName().mid(2).replace("_",",") + "," + ((QLineEdit *)obj)->text() + "\n";
   SendCommand(res.toStdString().c_str());
}

void MIPS::SendCommand(QString message)
{
    QString res;

    if (!serial->isOpen())  return;
    for(int i=0;i<2;i++)
    {
        rb.clear();
        serial->write(message.toStdString().c_str());
        rb.waitforline(500);
        if(rb.size() >= 1)
        {
            res = rb.getline();
            if(res == "") return;
            if(res == "?")
            {
                res = message + " :NAK";
                ui->statusBar->showMessage(res.toStdString().c_str());
                return;
            }
        }
    }
    res = message + " :Timeout";
    ui->statusBar->showMessage(res.toStdString().c_str(),500);
    return;
}

QString MIPS::SendMessage(QString message)
{
    QString res;

    if (!serial->isOpen())  return "";
    for(int i=0;i<2;i++)
    {
        rb.clear();
        serial->write(message.toStdString().c_str());
        rb.waitforline(500);
        if(rb.size() >= 1)
        {
            res = rb.getline();
            if(res != "") return res;
        }
    }
    res = message + " :Timeout";
    ui->statusBar->showMessage(res.toStdString().c_str(),500);
    res = "";
    return res;
}


void MIPS::mousePressEvent(QMouseEvent * event)
{
//    qDebug() << "event";
//    return;
    QMainWindow::mousePressEvent(event);
}


void MIPS::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);
   console->resize(ui->Terminal);
}

void MIPS::UpdateDCbias(void)
{
    QString res;

    ui->tabMIPS->setEnabled(false);
    ui->statusBar->showMessage(tr("Updating DC bias controls..."));
     // Read the number of channels and enable the proper controls
    res = SendMessage("GCHAN,DCB\n");
    ui->leGCHAN_DCB->setText(res);
    ui->gbDCbias1->setEnabled(false);
    ui->gbDCbias2->setEnabled(false);
    if(res.toInt() >= 8) ui->gbDCbias1->setEnabled(true);
    if(res.toInt() > 8) ui->gbDCbias2->setEnabled(true);
    res = SendMessage("GDCPWR\n");
    if(res == "ON") ui->chkPowerEnable->setChecked(true);
    else  ui->chkPowerEnable->setChecked(false);
    QObjectList widgetList = ui->gbDCbias1->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("le"))
       {
            res = "G" + w->objectName().mid(3).replace("_",",") + "\n";
            ((QLineEdit *)w)->setText(SendMessage(res));
       }
    }
    ui->tabMIPS->setEnabled(true);
    ui->statusBar->showMessage(tr(""));
}

void MIPS::DCbiasPower(void)
{
    if(ui->chkPowerEnable->isChecked()) SendCommand("SDCPWR,ON\n");
    else  SendCommand("SDCPWR,OFF\n");
}

void MIPS::MIPSdisconnect(void)
{
    closeSerialPort();
    ui->lblMIPSconfig->setText("");
}

void MIPS::MIPSconnect(void)
{
    QString res;

    if (serial->isOpen())  return;
    openSerialPort();
    disconnect(ui->comboRFchan, SIGNAL(currentTextChanged(QString)),0,0);
    ui->lblMIPSconfig->setText("MIPS: ");
    res = SendMessage("GVER\n");
    ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + res);
    ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\nModules present:");
    res = SendMessage("GCHAN,RF\n");
    if(res.contains("2")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   1 RF driver\n");
    if(res.contains("4")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   2 RF drivers\n");

    ui->comboRFchan->clear();
    for(int i=0;i<res.toInt();i++)
    {
        ui->comboRFchan->addItem(QString::number(i+1));
    }

    res = SendMessage("GCHAN,DCB\n");
    if(res.contains("8")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   1 DC bias (8 output channels)\n");
    if(res.contains("16")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   2 DC bias (16 output channels)\n");
    res = SendMessage("GCHAN,TWAVE\n");
    if(res.contains("1")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   TWAVE\n");
    res = SendMessage("GCHAN,FAIMS\n");
    if(res.contains("1")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   FAIMS\n");
    res = SendMessage("GCHAN,ESI\n");
    serial->write("GCHAN,ESI\n");
    if(res.contains("2")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   1 ESI\n");
    if(res.contains("4")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   2 ESI\n");
}

void MIPS::tabSelected()
{
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "System")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Terminal")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(console, SIGNAL(getData(QByteArray)),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2Console()));
        connect(console, SIGNAL(getData(QByteArray)), this, SLOT(writeData(QByteArray)));
        console->resize(ui->Terminal);
        console->setEnabled(true);
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Digital IO")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        UpdateDIO();
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "DCbias")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        UpdateDCbias();
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "RFdriver")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(ui->comboRFchan,SIGNAL(currentTextChanged(QString)),this,SLOT(UpdateRFdriver()));
        UpdateRFdriver();
    }
}

void MIPS::writeData(const QByteArray &data)
{
    serial->write(data);
}

void MIPS::readData2Console(void)
{
    QByteArray data = serial->readAll();
    console->putData(data);
}

void MIPS::readData2RingBuffer(void)
{
    int i;

    QByteArray data = serial->readAll();
    for(i=0;i<data.size();i++) rb.putch(data[i]);
}

void MIPS::openSerialPort()
{
    SettingsDialog::Settings p = settings->settings();
    serial->setPortName("cu." + p.name);
    serial->setBaudRate(p.baudRate);
    serial->setDataBits(p.dataBits);
    serial->setParity(p.parity);
    serial->setStopBits(p.stopBits);
    serial->setFlowControl(p.flowControl);
    if (serial->open(QIODevice::ReadWrite))
    {
            console->setEnabled(true);
            console->setLocalEchoEnabled(p.localEchoEnabled);
            ui->statusBar->showMessage(tr("Connected to %1 : %2, %3, %4, %5, %6")
                                       .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                                       .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"), serial->errorString());
        ui->statusBar->showMessage(tr("Open error"));
    }
}

void MIPS::closeSerialPort()
{
    if (serial->isOpen()) serial->close();
    console->setEnabled(false);
    ui->statusBar->showMessage(tr("Disconnected"));
}

void MIPS::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError)
    {
        QMessageBox::critical(this, tr("Critical Error"), serial->errorString());
        closeSerialPort();
    }
}

// Digital IO functions

// Slots

// Slot for Digital IO update button
void MIPS::UpdateDIO(void)
{
    QString res;

    ui->tabMIPS->setEnabled(false);
    ui->statusBar->showMessage(tr("Updating Digital IO controls..."));
    QObjectList widgetList = ui->gbDigitalOut->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("chk"))
       {
            res = "G" + w->objectName().mid(4).replace("_",",") + "\n";
            if(SendMessage(res).toInt()==1) ((QCheckBox *)w)->setChecked(true);
            else ((QCheckBox *)w)->setChecked(false);
       }
    }
    widgetList = ui->gbDigitalIn->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("chk"))
       {
            res = "G" + w->objectName().mid(4).replace("_",",") + "\n";
            if(SendMessage(res).toInt()==1) ((QCheckBox *)w)->setChecked(true);
            else ((QCheckBox *)w)->setChecked(false);
       }
    }
    ui->tabMIPS->setEnabled(true);
    ui->statusBar->showMessage(tr(""));
}

// Slot for Digital output check box selection
void MIPS::DOUpdated(void)
{
    QObject* obj = sender();
    QString res;

    res = obj->objectName().mid(3).replace("_",",") + ",";
    if(((QCheckBox *)obj)->checkState()) res += "1\n";
    else res+= "0\n";
    SendCommand(res.toStdString().c_str());
}

// Slot for Trigger high pushbutton
void MIPS::TrigHigh(void)
{
    SendCommand("TRIGOUT,HIGH\n");
}

// Slot for Trigger low pushbutton
void MIPS::TrigLow(void)
{
    SendCommand("TRIGOUT,LOW\n");
}

// Slot for Trigger pulse pushbutton
void MIPS::TrigPulse(void)
{
    SendCommand("TRIGOUT,PULSE\n");
}
// end Digital IO functions


// RF driver functions

// Slots

void MIPS::UpdateRFdriver(void)
{
    QString res;

    ui->tabMIPS->setEnabled(false);
    ui->statusBar->showMessage(tr("Updating RF driver controls..."));

    ui->leSRFFRQ->setText(SendMessage("GRFFRQ," + ui->comboRFchan->currentText() + "\n"));
    ui->leSRFDRV->setText(SendMessage("GRFDRV," + ui->comboRFchan->currentText() + "\n"));
    ui->leGRFPPVP->setText(SendMessage("GRFPPVP," + ui->comboRFchan->currentText() + "\n"));
    ui->leGRFPPVN->setText(SendMessage("GRFPPVN," + ui->comboRFchan->currentText() + "\n"));

    ui->tabMIPS->setEnabled(true);
    ui->statusBar->showMessage(tr(""));
}

// end RF driver functions

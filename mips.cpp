//
// MIPS
//
// This application is desiged to communicate with the MIPS system using a USB commection
// to the MIPS system. The MIPS system is controlled using a Arduino Due and the USB
// connection can be made to the native port or the programming port. If using the native
// port then the comm parameters are not important, if using the programming port set
// the baud rate to 115200, 8 bits, no parity, and 1 stop bit.
//
// This application support control and monitoring of most MIPS function for DCbias and
// digital IO function. Support is provided to create and edit pulse sequences.
//
// Note!
//  Opening the serial port on the native USB connection to the DUE and sentting the
//  data terminal ready lie to false will erase the DUE flash.
//          serial->setDataTerminalReady(false);
//
// Gordon Anderson
//
//  Revision history:
//  1.0, July 6, 2015
//      1.) Initial release
//  1.1, July 8, 2015
//      1.) Added MIPS firmware download function. Works on PC and MAC
//      2.) Removed the 1200 baud rate and all rates below 9600.
//  1.2, July 12, 2015
//      1.) Fixed the bugs in the firmware download feature.
//      2.) Tested on MAC and PC
//      3.) Updated a lot of screen issues from PC to MAC. Most features tested.
//  1.3, November 30, 2015
//      1.) Added socket support for networked MIPS.
//
//  To do list:
//  1.) Refactor the code, here are some to dos:
//      a.) Split psePoint out to its own file
//      b.) Add the build table code to the psePoint class
//      c.) Create a class to communicate with the MIPS system

//
#include "mips.h"
#include "ui_mips.h"
#include "console.h"
#include "settingsdialog.h"
#include "pse.h"
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
#include <QFileDialog>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include "qtcpsocket.h"


RingBuffer rb;

MIPS::MIPS(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MIPS)
{
    ui->setupUi(this);
    // Make the dialog fixed size.
    this->setFixedSize(this->size());

//  MIPS::setProperty("font", QFont("Times New Roman", 5));

    appPath = QApplication::applicationDirPath();
    pollTimer = new QTimer;
    console = new Console(ui->Terminal);
    console->setEnabled(false);
    serial = new QSerialPort(this);
    settings = new SettingsDialog;

    ui->actionClear->setEnabled(true);
    ui->actionOpen->setEnabled(true);
    ui->actionSave->setEnabled(true);
    ui->actionProgram_MIPS->setEnabled(true);

 //   connect(app, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(setWidgets(QWidget*, QWidget*)));    connect(ui->actionClear, SIGNAL(triggered()), console, SLOT(clear()));
    connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(loadSettings()));
    connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveSettings()));
    connect(ui->actionProgram_MIPS, SIGNAL(triggered()), this, SLOT(programMIPS()));
    connect(ui->pbConfigure, SIGNAL(pressed()), settings, SLOT(show()));
    connect(ui->tabMIPS,SIGNAL(currentChanged(int)),this,SLOT(tabSelected()));
    connect(ui->pbConnect,SIGNAL(pressed()),this,SLOT(MIPSconnect()));
    connect(ui->pbDisconnect,SIGNAL(pressed()),this,SLOT(MIPSdisconnect()));
//  connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this,SLOT(handleError(QSerialPort::SerialPortError)));
    connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
    connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
    connect(&client, SIGNAL(connected()),this, SLOT(connected()));
    connect(&client, SIGNAL(disconnected()),this, SLOT(disconnected()));
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollLoop()));
    connect(ui->actionAbout,SIGNAL(triggered(bool)), this, SLOT(DisplayAboutMessage()));
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
    // Setup the pulse sequence generation page
    ui->comboClock->clear();
    ui->comboClock->addItem("Ext");
    ui->comboClock->addItem("42000000");
    ui->comboClock->addItem("10500000");
    ui->comboClock->addItem("2625000");
    ui->comboClock->addItem("656250");
    ui->comboTrigger->clear();
    ui->comboTrigger->addItem("Software");
    ui->comboTrigger->addItem("Edge");
    ui->comboTrigger->addItem("Pos");
    ui->comboTrigger->addItem("Neg");
    ui->leSequenceNumber->setValidator(new QIntValidator);
    ui->leExternClock->setValidator(new QIntValidator);
    ui->leTimePoint->setValidator(new QIntValidator);
    ui->leValue->setValidator(new QIntValidator);
    // Sets the polling loop interval and starts the timer
    pollTimer->start(1000);
}

MIPS::~MIPS()
{
    delete settings;
    delete ui;
}

void MIPS::programMIPS(void)
{
    QString cmd;
    QString str;

    // Pop up a warning message and make sure the user wants to proceed
    QMessageBox msgBox;
    QString msg = "This will erase the MIPS firmware and attemp to load a new version. ";
    msg += "Make sure you have a new MIPS binary file to load, it should have a .bin extension.\n";
    msg += "The MIPS firmware will be erased so if your bin file is invalid or fails to program, MIPS will be rendered useless!\n";
    msgBox.setText(msg);
    msgBox.setInformativeText("Are you sure you want to contine?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if(ret == QMessageBox::No) return;
    // Select the Terminal tab and clear the display
    ui->tabMIPS->setCurrentIndex(1);
    // Select the binary file we are going to load to MIPS
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load MIPS firmware .bin file"),"",tr("Files (*.psg *.*)"));
    if(fileName == "") return;
    // Make sure MIPS is in ready state
    msg = "Unplug any RF drive heads from MIPS before you proceed. This includes unplugging the FAIMS RF deck as well. ";
    msg += "It is assumed that you have already established communications with the MIPS system. ";
    msg += "If the connection is not establised this function will exit with no action.";
    msgBox.setText(msg);
    msgBox.setInformativeText("");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    // Select the Terminal tab and clear the display
//    ui->tabMIPS->setCurrentIndex(1);
    console->clear();
    // Make sure the app is connected to MIPS
    if(!serial->isOpen())
    {
        console->putData("This application is not connected to MIPS!\n");
        return;
    }
    // Make sure we can locate the programmer tool...
    #if defined(Q_OS_MAC)
        cmd = appPath + "/bossac";
    #else
        cmd = appPath + "/bossac.exe";
    #endif
    QFileInfo checkFile(cmd);
    if (!checkFile.exists() || !checkFile.isFile())
    {
        console->putData("Can't find the programmer!\n");
        console->putData(cmd.toStdString().c_str());
        return;
    }
    // Erase MIPS's flash
    console->putData("MIPS is erased!\n");
    qDebug() << "erasing";
    closeSerialPort();
    QThread::sleep(1);
    while(serial->isOpen()) QApplication::processEvents();
    serial->setBaudRate(QSerialPort::Baud1200);
    QApplication::processEvents();
    serial->open(QIODevice::ReadWrite);
    serial->setDataTerminalReady(false);
    QThread::sleep(1);
    serial->close();
    QApplication::processEvents();
    QThread::sleep(5);
    // Download the bin file to MIPS and restart MIPS
    QApplication::processEvents();
    cmd = appPath + "/bossac -e -w -v -b " + fileName + " -R";
    console->putData(cmd.toStdString().c_str());
    console->putData("\n");
    QApplication::processEvents();
    QStringList arguments;
    arguments << "-c";
    arguments << cmd;
//    arguments << " -h";
    #if defined(Q_OS_MAC)
        process.start("/bin/bash",arguments);
    #else
        process.start(cmd);
    #endif
    console->putData("Download should start soon...\n");
    connect(&process,SIGNAL(readyReadStandardOutput()),this,SLOT(readProcessOutput()));
    connect(&process,SIGNAL(readyReadStandardError()),this,SLOT(readProcessOutput()));
}

void MIPS::readProcessOutput(void)
{
    console->putData(process.readAllStandardOutput());
    console->putData(process.readAllStandardError());
}

void MIPS::DisplayAboutMessage(void)
{
    QMessageBox::information(
        this,
        tr("Application Named"),
        tr("MIPS interface application, written by Gordon Anderson. This application allows communications with the MIPS system supporting monitoring and control as well as pulse sequence generation.") );
}

void MIPS::loadSettings(void)
{
    QStringList resList;

    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Pulse Sequence File"),"",tr("Files (*.psg *.*)"));
    if(fileName == "") return;
    QObjectList widgetList = ui->gbDCbias1->children();
    widgetList += ui->gbDCbias2->children();
    widgetList += ui->gbDigitalOut->children();
    QFile file(fileName);
    if(file.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        // We're going to streaming the file
        // to the QString
        QTextStream stream(&file);

        QString line;
        do
        {
            line = stream.readLine();
            resList = line.split(",");
            if(resList.count() == 2)
            {
//                qDebug() << resList[0] << resList[1];
                foreach(QObject *w, widgetList)
                {
                    if(w->objectName().mid(0,3) == "leS")
                    {
                        if(resList[1] != "") if(w->objectName() == resList[0])
                        {
                            ((QLineEdit *)w)->setText(resList[1]);
                            QMetaObject::invokeMethod(w, "editingFinished");
                        }
                    }
                    if(w->objectName().mid(0,4) == "chkS")
                    {
                        if(w->objectName() == resList[0])
                        {
                            if(resList[1] == "true")
                            {
                                ((QCheckBox *)w)->setChecked(true);
                            }
                            else ((QCheckBox *)w)->setChecked(false);
                        }
                    }
                }

            }
        } while(!line.isNull());
        file.close();
        ui->statusBar->showMessage("Settings loaded to " + fileName,2000);
    }
}

void MIPS::saveSettings(void)
{
    QString res;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save to Settings File"),"",tr("Files (*.settings)"));
    if(fileName == "") return;
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        // We're going to streaming text to the file
        QTextStream stream(&file);

        QObjectList widgetList = ui->gbDCbias1->children();
        widgetList += ui->gbDCbias2->children();
        widgetList += ui->gbDigitalOut->children();
        foreach(QObject *w, widgetList)
        {
            if(w->objectName().mid(0,3) == "leS")
            {
                res = w->objectName() + "," + ((QLineEdit *)w)->text() + "\n";
                stream << res;
            }
            if(w->objectName().mid(0,4) == "chkS")
            {
                res = w->objectName() + ",";
                if(((QCheckBox *)w)->checkState()) res += "true\n";
                else res += "false\n";
                stream << res;
            }
        }
        file.close();
        ui->statusBar->showMessage("Settings saved to " + fileName,2000);
    }
}

void MIPS::pollLoop(void)
{
    QString res ="";
    //char c;

    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Pulse Sequence Generation")
    {
        /*
        while(true)
        {
            c = rb.getch();
            if((int)c==0) return;
            ui->statusBar->showMessage(ui->statusBar->currentMessage() + c,2000);
        }
        */
    }
}

int MIPS::Referenced(QList<psgPoint*> P, int i)
{
    int j;

    for(j = 0; j < P.size(); j++)
    {
        if(P[j]->Loop)
        {
            if(P[j]->LoopName == P[i]->Name) return(P[j]->LoopCount);
        }
    }
    return - 1;
}

QString MIPS::BuildTableCommand(QList<psgPoint*> P)
{
    QString sTable;
    QString TableName;
    int i,j,Count;
    bool NoChange;

    sTable = "";
    TableName = "A";
    for(i = 0;i<P.size();i++)
    {
        NoChange = true;
        // Assume non zero values need to be sent at time point 0
        if(i == 0)
        {
            // See if any time point loops to this location
            Count = Referenced(P, i);
            if(Count >= 0)
            {
                // Here if this time point is referenced so set it up
                sTable += "0:[" + TableName;
                sTable += ":" + QString::number(Count) + "," + QString::number(P[i]->TimePoint);
                TableName = QString::number((int)TableName.mid(1,1).toStdString().c_str()[0] + 1);
            }
            else sTable += QString::number(P[i]->TimePoint);
            for(j = 0; j < 16; j++)
            {
                if(P[0]->DCbias[j] != 0)
                {
                    sTable += ":" + QString::number(j + 1) + ":" + QString::number(P[0]->DCbias[j]);
                    NoChange = false;
                }
            }
            for(j = 0; j < 16;j++)
            {
                if(P[0]->DigitalO[j])
                {
                    sTable += ":" + QString((int)'A' + j) + ":1";
                    NoChange = false;
                }
            }
            // If nothing changed then update DO A, something has to be defined at this time point
            if(NoChange)
            {
                if(P[i]->DigitalO[0]) sTable += ":A:1";
                else sTable += ":A:0";
            }
        }
        else
        {
            // See if any time point loops to this location
            NoChange = true;
            Count = Referenced(P, i);
            if (Count >= 0)
            {
                // Here if this time point is referenced so set it up
                sTable += "," + QString::number(P[i]->TimePoint) + ":[" + TableName + ":";
                sTable += QString::number(Count) + ",0";
                TableName = QString::number((int)TableName.mid(1,1).toStdString().c_str()[0] + 1);
            }
                else sTable += "," + QString::number(P[i]->TimePoint);
                for(j = 0; j < 16; j++)
                {
                    if(P[i]->DCbias[j] != P[i - 1]->DCbias[j])
                    {
                        sTable += ":" + QString::number(j + 1) + ":" + QString::number(P[i]->DCbias[j]);
                        NoChange = false;
                    }
                }
                for(j = 0; j < 16; j++)
                {
                    if(P[i]->DigitalO[j] != P[i - 1]->DigitalO[j])
                    {
                        if (P[i]->DigitalO[j]) sTable += ":" + QString((int)'A' + j) + ":1";
                        else sTable += ":" + QString((int)'A' + j) + ":0";
                    }
                    NoChange = false;
                }
                // If nothing changed then update DO A, something has to be defined at this time point
                if(NoChange)
                {
                    if(P[i]->DigitalO[0]) sTable += ":A:1";
                    else sTable += ":A:0";
                }
                if(P[i]->Loop) sTable += "]";
            }
    }
    sTable = "STBLDAT;" + sTable + ";";
    return sTable;
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

    if (!serial->isOpen() && !client.isOpen())
    {
        ui->statusBar->showMessage("Disconnected!",2000);
        return;
    }
    for(int i=0;i<2;i++)
    {
        rb.clear();
        if (serial->isOpen()) serial->write(message.toStdString().c_str());
        if (client.isOpen()) client.write(message.toStdString().c_str());
        rb.waitforline(500);
        if(rb.size() >= 1)
        {
            res = rb.getline();
            if(res == "") return;
            if(res == "?")
            {
                res = message + " :NAK";
                ui->statusBar->showMessage(res.toStdString().c_str(),2000);
                return;
            }
        }
    }
    res = message + " :Timeout";
    ui->statusBar->showMessage(res.toStdString().c_str(),2000);
    return;
}

QString MIPS::SendMessage(QString message)
{
    QString res;

    if (!serial->isOpen() && !client.isOpen())
    {
        ui->statusBar->showMessage("Disconnected!",2000);
        return "";
    }
     for(int i=0;i<2;i++)
    {
        rb.clear();
        if (serial->isOpen()) serial->write(message.toStdString().c_str());
        if (client.isOpen()) client.write(message.toStdString().c_str());
        rb.waitforline(500);
        if(rb.size() >= 1)
        {
            res = rb.getline();
            if(res != "") return res;
        }
    }
    res = message + " :Timeout";
    ui->statusBar->showMessage(res.toStdString().c_str(),2000);
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

   ui->tabMIPS->setFixedWidth(MIPS::width());
   #if defined(Q_OS_MAC)
    ui->tabMIPS->setFixedHeight(MIPS::height()-(ui->statusBar->height()));
   #else
    // Not sure why I need this 3x for a windows system??
    ui->tabMIPS->setFixedHeight(MIPS::height()-(ui->statusBar->height()*3));
   #endif

   console->resize(ui->Terminal);
   QMainWindow::resizeEvent(event);
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
    if(client.isOpen()) client.close();
    closeSerialPort();
    ui->lblMIPSconfig->setText("");
    ui->lblMIPSconnectionNotes->setHidden(false);
}

void MIPS::MIPSsetup(void)
{
    QString res;

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
    //serial->write("GCHAN,ESI\n");
    if(res.contains("2")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   1 ESI\n");
    if(res.contains("4")) ui->lblMIPSconfig->setText(ui->lblMIPSconfig->text() + "\n   2 ESI\n");
}

// Here when the Connect push button is pressed. This function makes a connection with MIPS.
// If a network name or IP is provided that connection is tried first.
// If the serial port of socket is connected then this function exits.
void MIPS::MIPSconnect(void)
{
    QTime timer;
    QString res;

    if(client.isOpen() || serial->isOpen()) return;

    if(ui->leMIPSnetName->text() != "")
    {
       client_connected = false;
       client.connectToHost(ui->leMIPSnetName->text(), 2015);
       ui->statusBar->showMessage(tr("Connecting..."));
       timer.start();
       while(timer.elapsed() < 10000)
       {
           QApplication::processEvents();
           if(client_connected)
           {
               MIPSsetup();
               ui->lblMIPSconnectionNotes->setHidden(true);
               return;
           }
       }
       ui->statusBar->showMessage(tr("MIPS failed to connect!"));
       client.abort();
       client.close();
       return;
    }
    else
    {
       openSerialPort();
       serial->setDataTerminalReady(true);
    }
    MIPSsetup();
}

void MIPS::tabSelected()
{
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "System")
    {
        settings->fillPortsInfo();
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Terminal")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        disconnect(console, SIGNAL(getData(QByteArray)),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2Console()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2Console()));
        connect(console, SIGNAL(getData(QByteArray)), this, SLOT(writeData(QByteArray)));
        console->resize(ui->Terminal);
        console->setEnabled(true);
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Digital IO")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        UpdateDIO();
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "DCbias")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        UpdateDCbias();
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "RFdriver")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(ui->comboRFchan,SIGNAL(currentTextChanged(QString)),this,SLOT(UpdateRFdriver()));
        UpdateRFdriver();
    }
    if( ui->tabMIPS->tabText(ui->tabMIPS->currentIndex()) == "Pulse Sequence Generation")
    {
        disconnect(serial, SIGNAL(readyRead()),0,0);
        disconnect(&client, SIGNAL(readyRead()),0,0);
        connect(serial, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        connect(&client, SIGNAL(readyRead()), this, SLOT(readData2RingBuffer()));
        UpdatePSG();
    }
}

void MIPS::writeData(const QByteArray &data)
{
    if(client.isOpen()) client.write(data);
    if(serial->isOpen()) serial->write(data);
}

void MIPS::readData2Console(void)
{
    QByteArray data;

    if(serial->isOpen()) data = serial->readAll();
    if(client.isOpen()) data = client.readAll();
    console->putData(data);
}

void MIPS::readData2RingBuffer(void)
{
    int i;

    if(client.isOpen())
    {
        QByteArray data = client.readAll();
        for(i=0;i<data.size();i++) rb.putch(data[i]);
    }
    if(serial->isOpen())
    {
        QByteArray data = serial->readAll();
        for(i=0;i<data.size();i++) rb.putch(data[i]);
    }
}

void MIPS::openSerialPort()
{
    connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this,SLOT(handleError(QSerialPort::SerialPortError)));
    SettingsDialog::Settings p = settings->settings();
    serial->setPortName(p.name);
    #if defined(Q_OS_MAC)
        serial->setPortName("cu." + p.name);
    #endif
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
    disconnect(serial, SIGNAL(error(QSerialPort::SerialPortError)),0,0);
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

// Pulse sequence generator functions

void MIPS::UpdatePSG(void)
{
    ui->tabMIPS->setEnabled(false);
    ui->statusBar->showMessage(tr("Updating Pulse Sequence Generation controls..."));

    if(SendMessage("GTBLADV\n").contains("ON")) ui->chkAutoAdvance->setChecked(true);
    else ui->chkAutoAdvance->setChecked(false);
    ui->leSequenceNumber->setText(SendMessage("GTBLNUM\n"));

    ui->tabMIPS->setEnabled(true);
    ui->statusBar->showMessage(tr(""));
}

void MIPS::on_pbDownload_pressed(void)
{
    QString res;

    // Make sure a table is loaded
    ui->pbDownload->setDown(false);
    if(psg.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("There is no Pulse Sequence to download to MIPS!");
        msgBox.exec();
        return;
    }
    // Make sure system is in local mode
    SendCommand("SMOD,LOC\n");
    // Set clock
    SendCommand("STBLCLK," + ui->comboClock->currentText().toUpper() + "\n");
    // Set trigger
    res = ui->comboTrigger->currentText().toUpper();
    if(res == "SOFTWARE") res = "SW";
    SendCommand("STBLTRG," + res + "\n");
    // Send table
    SendCommand(BuildTableCommand(psg));
    // Put system in table mode
    SendCommand("SMOD,TBL\n");
    rb.waitforline(100);
    ui->statusBar->showMessage(rb.getline());
}

void MIPS::on_pbViewTable_pressed()
{
    QString table;
    QList<psgPoint> P;

    ui->pbViewTable->setDown(false);
    if(psg.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("There is no Pulse Sequence to view!");
        msgBox.exec();
        return;
    }

    table = BuildTableCommand(psg);
    QMessageBox msgBox;
    msgBox.setText(table);
    msgBox.exec();
}

void MIPS::on_pbLoadFromFile_pressed()
{
    psgPoint *P;

    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Pulse Sequence File"),"",tr("Files (*.psg *.*)"));

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);

    psg.clear();
    while(!in.atEnd())
    {
        P = new psgPoint;
        in >> *P;
        psg.push_back(P);
    }
    file.close();
    ui->pbLoadFromFile->setDown(false);
}

void MIPS::on_pbCreateNew_pressed()
{
    psgPoint *point = new psgPoint;

    ui->pbCreateNew->setDown(false);
    if(psg.size() > 0)
    {
        QMessageBox msgBox;
        msgBox.setText("This will overwrite the current Pulse Sequence Table.");
        msgBox.setInformativeText("Do you want to contine?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        if(ret == QMessageBox::No) return;
    }
    point->Name = "TP_1";
    psg.clear();
    psg.push_back(point);
    pse = new pseDialog(&psg);
    pse->show();
}

void MIPS::on_pbSaveToFile_pressed()
{
    QList<psgPoint*>::iterator it;

    ui->pbSaveToFile->setDown(false);
    if(psg.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("There is no Pulse Sequence to save!");
        msgBox.exec();
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save to Pulse Sequence File"),"",tr("Files (*.psg)"));
    QFile file(fileName);
    file.open(QIODevice::WriteOnly);

    QDataStream out(&file);   // we will serialize the data into the file
    for(it = psg.begin(); it != psg.end(); ++it) out << **it;

    file.close();
    ui->pbSaveToFile->setDown(false);
}


void MIPS::on_pbEditCurrent_pressed()
{
    ui->pbEditCurrent->setDown(false);
    if(psg.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("There is no Pulse Sequence to edit!");
        msgBox.exec();
        return;
    }
    pse = new pseDialog(&psg);
    pse->show();
}

void MIPS::on_leSequenceNumber_textEdited(const QString &arg1)
{
    if(arg1 == "") return;
    SendCommand("STBLNUM," + arg1 + "\n");
}

void MIPS::on_chkAutoAdvance_clicked(bool checked)
{
    if(checked) SendCommand("STBLADV,ON\n");
    else SendCommand("STBLADV,OFF\n");
}

void MIPS::on_pbTrigger_pressed()
{
    SendCommand("TBLSTRT\n");
    rb.waitforline(100);
    ui->statusBar->showMessage(rb.getline());
    rb.waitforline(500);
    ui->statusBar->showMessage(ui->statusBar->currentMessage() + " " + rb.getline());
    rb.waitforline(100);
    ui->statusBar->showMessage(ui->statusBar->currentMessage() + " " + rb.getline());
}

void MIPS::on_leSRFFRQ_editingFinished()
{
    if(!ui->leSRFFRQ->isModified()) return;
    SendCommand("SRFFRQ," + ui->comboRFchan->currentText() + "," + ui->leSRFFRQ->text() + "\n");
    ui->leSRFFRQ->setModified(false);
}

void MIPS::on_leSRFDRV_editingFinished()
{
    if(!ui->leSRFDRV->isModified()) return;
    SendCommand("SRFDRV," + ui->comboRFchan->currentText() + "," + ui->leSRFDRV->text() + "\n");
    ui->leSRFDRV->setModified(false);
}

void MIPS::on_pbRead_pressed()
{
    ui->leValue->setText(SendMessage("GTBLVLT," + ui->leTimePoint->text() + "," + ui->leChannel->text() + "\n"));
}

void MIPS::on_pbWrite_pressed()
{
    SendCommand("STBLVLT," + ui->leTimePoint->text() + "," + ui->leChannel->text() + "," + ui->leValue->text() + "\n");
}

void MIPS::connected(void)
{
    ui->statusBar->showMessage(tr("MIPS connected"));
    client_connected = true;
}

void MIPS::disconnected(void)
{
    ui->statusBar->showMessage(tr("Disconnected"));
}

void MIPS::setWidgets(QWidget* old, QWidget* now)
{
}


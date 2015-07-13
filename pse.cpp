#include "pse.h"
#include "ui_pse.h"

#include <QDebug>
#include <QMessageBox>
#include <QFile>

QT_USE_NAMESPACE

//static const char blankString[] = QT_TRANSLATE_NOOP("pseDialog", "N/A");

QDataStream &operator<<(QDataStream &out, const psgPoint &point)
{
    int i;

    out << point.Name << quint32(point.TimePoint);
    for(i=0;i<16;i++) out << point.DigitalO[i];
    for(i=0;i<16;i++) out << point.DCbias[i];
    out << point.Loop << point.LoopName << quint32(point.LoopCount);
    return out;
}

QDataStream &operator>>(QDataStream &in, psgPoint &point)
{
    QString Name;
    quint32 TimePoint;
    bool    DigitalO[16];
    float   DCbias[16];
    bool    Loop;
    QString LoopName;
    quint32 LoopCount;
    int     i;

    in >> Name;
    in >> TimePoint;
    for(i=0;i<16;i++) in >> DigitalO[i];
    for(i=0;i<16;i++) in >> DCbias[i];
    in >> Loop >> LoopName >> LoopCount;

    point.Name = Name;
    point.TimePoint = TimePoint;
    for(i=0;i<16;i++) point.DigitalO[i] = DigitalO[i];
    for(i=0;i<16;i++) point.DCbias[i] = DCbias[i];
    point.Loop = Loop;
    point.LoopName = LoopName;
    point.LoopCount = LoopCount;

    return in;
}

psgPoint::psgPoint()
{
    int  i;

    for(i=0;i<16;i++)
    {
        DigitalO[i] = false;
        DCbias[i] = 0.0;
    }
    Loop = false;
    LoopCount = 0;
    Name = "";
    LoopName = "";
    TimePoint = 0;
}

pseDialog::pseDialog(QList<psgPoint *> *psg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pseDialog)
{
    ui->setupUi(this);
    // Make the dialog fixed size.
    this->setFixedSize(this->size());

    qDebug() << psg->size();
    p = psg;
    activePoint = (*psg)[0];
    CurrentIndex = 0;
    UpdateDialog(activePoint);

//    pseDialog::setProperty("font", QFont("Times New Roman", 5));
    QObjectList widgetList = ui->gbDigitalOut->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("chkDO"))
       {
           connect(((QCheckBox *)w),SIGNAL(clicked(bool)),this,SLOT(on_DIO_checked()));
       }
    }
    ui->gbCurrentPoint->setTitle("Current time point: " + QString::number(CurrentIndex+1) + " of " + QString::number(p->size()));
    widgetList = ui->gbDCbias->children();
    foreach(QObject *w, widgetList)
    {
       if(w->objectName().contains("leDCB"))
       {
           connect(((QLineEdit *)w),SIGNAL(textEdited(QString)),this,SLOT(on_DCBIAS_edited()));
       }
    }
}

pseDialog::~pseDialog()
{
    delete ui;
}

void pseDialog::on_DCBIAS_edited()
{
    QObject* obj = sender();

    int Index = obj->objectName().mid(5).toInt() - 1;
    activePoint->DCbias[Index] = ((QLineEdit *)obj)->text().toFloat();
}

void pseDialog::on_DIO_checked()
{
    QObject* obj = sender();

    int Index = (int)obj->objectName().mid(5,1).toStdString().c_str()[0] - (int)'A';
    if(((QCheckBox *)obj)->isChecked()) activePoint->DigitalO[Index] = true;
    else  activePoint->DigitalO[Index] = false;
}

void pseDialog::UpdateDialog(psgPoint *point)
{
    QList<psgPoint*>::iterator it;
    int  Index;
    QString temp;

   temp = point->LoopName;
   ui->comboLoop->clear();
   ui->comboLoop->addItem("");
   for(it = p->begin(); it != p->end(); ++it) if(*it == point) break;
   else ui->comboLoop->addItem((*it)->Name);
   Index = ui->comboLoop->findText(temp);
   if( Index != -1) ui->comboLoop->setCurrentIndex(Index);
   ui->leName->setText(point->Name);
   ui->leClocks->setText(QString::number(point->TimePoint));
   ui->leCycles->setText(QString::number(point->LoopCount));
   if(point->Loop) ui->chkLoop->setChecked(true);
   else ui->chkLoop->setChecked(false);
   ui->comboLoop->setCurrentText(point->LoopName);
   QObjectList widgetList = ui->gbDigitalOut->children();
   foreach(QObject *w, widgetList)
   {
      if(w->objectName().contains("chkDO"))
      {
          Index = (int)w->objectName().mid(5,1).toStdString().c_str()[0] - (int)'A';
          ((QCheckBox *)w)->setChecked(point->DigitalO[Index]);
      }
   }
   widgetList = ui->gbDCbias->children();
   foreach(QObject *w, widgetList)
   {
      if(w->objectName().contains("leDCB"))
      {
          Index = w->objectName().mid(5).toInt() - 1;
          ((QLineEdit *)w)->setText(QString::number(point->DCbias[Index]));
      }
   }
}

void pseDialog::on_pbNext_pressed()
{
    if(CurrentIndex < p->size() - 1) CurrentIndex++;
    activePoint = (*p)[CurrentIndex];
    UpdateDialog(activePoint);
    ui->gbCurrentPoint->setTitle("Current time point: " + QString::number(CurrentIndex+1) + " of " + QString::number(p->size()));
}

void pseDialog::on_pbPrevious_pressed()
{
     if(CurrentIndex > 0) CurrentIndex--;
     activePoint = (*p)[CurrentIndex];
     UpdateDialog(activePoint);
     ui->gbCurrentPoint->setTitle("Current time point: " + QString::number(CurrentIndex+1) + " of " + QString::number(p->size()));
}

// Insert a new point after the current point
void pseDialog::on_pbInsert_pressed()
{
    psgPoint *point = new psgPoint;
    QList<psgPoint*>::iterator it;
    int i;

    *point = *activePoint;
    // Generate the new time point's name. If the previous timepoint name ends with a
    // _number incement the number and see if the new name is unigue. If it is use it,
    // else append a _1 to the previous name.
    if((i = point->Name.lastIndexOf("_")) < 0)
    {
        point->Name += "_1";
    }
    else
    {
        // Here if _ was found so we assume a number follows
        point->Name = point->Name.mid(0,i+1) + QString::number(point->Name.mid(i+1).toInt()+1);
    }
    // See if this name is unique
    for(it = p->begin(); it != p->end(); ++it) if((*it)->Name == point->Name)
    {
        point->Name = activePoint->Name + "_1";
        break;
    }
    for(it = p->begin(); it != p->end(); ++it) if(*it == activePoint) break;
    it++;
    p->insert(it, point);
    CurrentIndex++;
    activePoint = point;
    UpdateDialog(activePoint);
    ui->gbCurrentPoint->setTitle("Current time point: " + QString::number(CurrentIndex+1) + " of " + QString::number(p->size()));
}

// Delete the current point
void pseDialog::on_pbDelete_pressed()
{
    QList<psgPoint*>::iterator it,itnext;

    if(p->size() <= 1)
    {
        QMessageBox::information(NULL, "Error!", "Can't delete the only point in the sequence!");
        ui->pbDelete->setDown(false);
        return;
    }
    it = p->begin();
    for(it = p->begin(); it != p->end(); ++it) if(*it == activePoint) break;
    itnext =p->erase(it);
    activePoint = *itnext;
    CurrentIndex=0;
    for(it = p->begin(); it != p->end(); ++it,++CurrentIndex) if(*it == activePoint) break;
    if(CurrentIndex >= p->size()) CurrentIndex = p->size() - 1;
    activePoint = (*p)[CurrentIndex];
    UpdateDialog(activePoint);
    ui->gbCurrentPoint->setTitle("Current time point: " + QString::number(CurrentIndex+1) + " of " + QString::number(p->size()));
}

void pseDialog::on_leName_textChanged(const QString &arg1)
{
    activePoint->Name = arg1;
}

void pseDialog::on_leClocks_textChanged(const QString &arg1)
{
   activePoint->TimePoint = arg1.toInt();
}

void pseDialog::on_leCycles_textChanged(const QString &arg1)
{
    activePoint->LoopCount = arg1.toInt();
}

void pseDialog::on_chkLoop_clicked(bool checked)
{
    if(checked) activePoint->Loop = true;
    else activePoint->Loop = false;
}

void pseDialog::on_comboLoop_currentIndexChanged(const QString &arg1)
{
    activePoint->LoopName = arg1;
}

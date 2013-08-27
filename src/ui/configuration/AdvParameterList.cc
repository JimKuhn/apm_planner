#include "AdvParameterList.h"
#include "QsLog.h"
#include <QTableWidgetItem>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
AdvParameterList::AdvParameterList(QWidget *parent) : AP2ConfigWidget(parent),
    m_paramDownloadState(starting),
    m_paramDownloadCount(0)
{
    ui.setupUi(this);
    connect(ui.refreshPushButton,SIGNAL(clicked()),this,SLOT(refreshButtonClicked()));
    connect(ui.writePushButton,SIGNAL(clicked()),this,SLOT(writeButtonClicked()));
    connect(ui.loadPushButton,SIGNAL(clicked()),this,SLOT(loadButtonClicked()));
    connect(ui.savePushButton,SIGNAL(clicked()),this,SLOT(saveButtonClicked()));
    connect(ui.tableWidget,SIGNAL(itemChanged(QTableWidgetItem*)),this,SLOT(tableWidgetItemChanged(QTableWidgetItem*)));

    ui.tableWidget->setColumnCount(4);
    //ui.tableWidget->horizontalHeader()->hide();
    ui.tableWidget->verticalHeader()->hide();
    ui.tableWidget->setColumnWidth(0,200);
    ui.tableWidget->setColumnWidth(1,100);
    ui.tableWidget->setColumnWidth(2,100);
    ui.tableWidget->setColumnWidth(3,800);
    ui.tableWidget->setHorizontalHeaderItem(0,new QTableWidgetItem("Param"));
    ui.tableWidget->setHorizontalHeaderItem(1,new QTableWidgetItem("Value"));
    ui.tableWidget->setHorizontalHeaderItem(2,new QTableWidgetItem("Unit"));
    ui.tableWidget->setHorizontalHeaderItem(3,new QTableWidgetItem("Description"));
    ui.tableWidget->horizontalHeaderItem (3)->setTextAlignment(Qt::AlignLeft);

    ui.paramProgressBar->setRange(0,100);

    initConnections();
}
void AdvParameterList::tableWidgetItemChanged(QTableWidgetItem* item)
{
    if (!ui.tableWidget->item(item->row(),1))
    {
        //Invalid item, something has gone awry.
        return;
    }
    m_origBrushList.append(ui.tableWidget->item(item->row(),0)->text());
    QBrush brush = QBrush(QColor::fromRgb(255,100,100));
    item->setBackground(brush);
    m_modifiedParamMap[ui.tableWidget->item(item->row(),0)->text()] = item->text().toDouble();
}
void AdvParameterList::writeButtonClicked()
{
    if (!m_uas)
    {
        showNullMAVErrorMessageBox();
        return;
    }
    for (QMap<QString,double>::const_iterator i = m_modifiedParamMap.constBegin();i!=m_modifiedParamMap.constEnd();i++)
    {
        m_uas->getParamManager()->setParameter(1,i.key(),i.value());
    }
    m_modifiedParamMap.clear();
}

AdvParameterList::~AdvParameterList()
{
}
void AdvParameterList::refreshButtonClicked()
{
    if (!m_uas)
    {
        showNullMAVErrorMessageBox();
        return;
    }
    m_uas->getParamManager()->requestParameterList();
    m_paramDownloadState = starting;
}

void AdvParameterList::setParameterMetaData(QString name,QString humanname,QString description,QString unit)
{
    m_paramToNameMap[name] = humanname;
    m_paramToDescriptionMap[name] = description;
    m_paramToUnitMap[name] = unit;
}
void AdvParameterList::loadButtonClicked()
{
    if (!m_uas)
    {
        showNullMAVErrorMessageBox();
        return;
    }
    QString filename = QFileDialog::getOpenFileName(this,"Open File");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::information(this,"Error","Unable to open file");
        return;
    }
    QString filestr = file.readAll();
    file.close();
    QStringList filesplit = filestr.split("\r\n");
    //QProgressDialog dialog("Loading parameters...","",0,filesplit.size());
    //dialog.setWindowModality(Qt::WindowModal);
    foreach (QString fileline,filesplit)
    {
        if (fileline.startsWith("#"))
        {
            //Comment
        }
        else
        {
            QStringList linesplit = fileline.split(",");
            if (linesplit.size() == 2)
            {
                //linesplit[0] == name
                //linesplit[1] == value as a double.
                m_uas->getParamManager()->setParameter(1,linesplit[0],linesplit[1].toDouble());

            }
        }
        //dialog.setValue(dialog.value()+1);
    }
}

void AdvParameterList::saveButtonClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,"Save File");
    QFile file(filename);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
    {
        QMessageBox::information(this,"Error","Unable to open file");
        return;
    }
    QString fileheader = QInputDialog::getText(this,"Input file header","Header at beginning of file:");

    file.write(QString("#NOTE: " + QDateTime::currentDateTime().toString("M/d/yyyy h:m:s AP") + ": " + fileheader + "\r\n").toAscii());
    //QMap<QString,QTableWidgetItem*> m_paramValueMap;
    for (QMap<QString,QTableWidgetItem*>::const_iterator i = m_paramValueMap.constBegin();i!=m_paramValueMap.constEnd();i++)
    {
        file.write(QString(i.key()).append(",").append(i.value()->text()).append("\r\n").toAscii());
    }
    file.flush();
    file.close();
}

void AdvParameterList::parameterChanged(int uas, int component, QString parameterName, QVariant value)
{
    disconnect(ui.tableWidget,SIGNAL(itemChanged(QTableWidgetItem*)),this,SLOT(tableWidgetItemChanged(QTableWidgetItem*)));
    if (!m_paramValueMap.contains(parameterName))
    {
        ui.tableWidget->setRowCount(ui.tableWidget->rowCount()+1);

        //Col 0, param name
        QTableWidgetItem *paramnameitem = new QTableWidgetItem(parameterName);
        paramnameitem->setFlags(paramnameitem->flags() ^ Qt::ItemIsEditable);
        ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,0,paramnameitem);

        //Col 1, param value
        QTableWidgetItem *valitem = new QTableWidgetItem(QString::number(value.toFloat(),'f',2));
        valitem->setFlags(valitem->flags() | Qt::ItemIsEditable);
        ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,1,valitem);

        //Col 2, param unit
        if (m_paramToUnitMap.contains(parameterName))
        {
            QTableWidgetItem *item = new QTableWidgetItem(m_paramToUnitMap[parameterName]);
            item->setFlags(item->flags() ^ Qt::ItemIsEditable);
            ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,item);
        }
        else
        {
            QTableWidgetItem *item = new QTableWidgetItem("");
            item->setFlags(item->flags() ^ Qt::ItemIsEditable);
            ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,2,item);
        }

        //Col 3, description
        QString desc = "";
        if (m_paramToNameMap.contains(parameterName))
        {
            desc += m_paramToNameMap[parameterName] + " - ";
        }
        else
        {
            desc = "";
        }

        if (m_paramToDescriptionMap.contains(parameterName))
        {
            desc += m_paramToDescriptionMap[parameterName];
        }
        QTableWidgetItem *item = new QTableWidgetItem(desc);
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        ui.tableWidget->setItem(ui.tableWidget->rowCount()-1,3,item);


        m_paramValueMap[parameterName] = ui.tableWidget->item(ui.tableWidget->rowCount()-1,1);
        ui.tableWidget->sortByColumn(0,Qt::AscendingOrder);
    }
    if (m_origBrushList.contains(parameterName))
    {
        m_paramValueMap[parameterName]->setBackground(QBrush());
        m_origBrushList.removeAll(parameterName);
    }
    m_paramValueMap[parameterName]->setText(QString::number(value.toFloat(),'f',2));
    connect(ui.tableWidget,SIGNAL(itemChanged(QTableWidgetItem*)),this,SLOT(tableWidgetItemChanged(QTableWidgetItem*)));

}

void AdvParameterList::parameterChanged(int uas, int component, int parameterCount, int parameterId, QString parameterName, QVariant value)
{
    QString countString;
    // Create progress of downloading all parameters for UI
    switch (m_paramDownloadState){
    case starting:
        QLOG_DEBUG() << "Starting Param Progress Bar Updating sys:" << uas;
        m_paramDownloadCount = 1;

        countString = QString::number(m_paramDownloadCount) + "/"
                        + QString::number(parameterCount);
        QLOG_DEBUG() << "Param Progress Bar: " << countString
                     << "paramId:" << parameterId << "name:" << parameterName;
        ui.progressLabel->setText(countString);
        ui.paramProgressBar->setValue((m_paramDownloadCount/(float)parameterCount)*100.0);

        m_paramDownloadState = refreshing;
        break;

    case refreshing:
        m_paramDownloadCount++;
        countString = QString::number(m_paramDownloadCount) + "/"
                        + QString::number(parameterCount);
        QLOG_DEBUG() << "Param Progress Bar: " << countString
                     << "paramId:" << parameterId << "name:" << parameterName;
        ui.progressLabel->setText(countString);
        ui.paramProgressBar->setValue((m_paramDownloadCount/(float)parameterCount)*100.0);

        if (m_paramDownloadCount == parameterCount)
            m_paramDownloadState = completed;
        break;

    case completed:
        QLOG_DEBUG() << "Finished Downloading Params" << m_paramDownloadCount;
        m_paramDownloadState = none;
        break;

    case none:
    default:
        ; // Do Nothing
    }
}


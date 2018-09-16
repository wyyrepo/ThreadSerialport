#include "myconnect.h"
#include <QObject>
#include <QApplication>
#include "mainwindow.h"
#include <QThread>
#include <windows.h>
#include "qmythreadmanager.h"

QMap< int , MyConnect* > g_ConnMap;
MyConnect::MyConnect(CRTUObject *pRtuObj/*QString *pstrCommType,
                     QStringList *pstrListSerialPara,
                     QStringList *pstrListNetPara*/ )
                    : m_pConnThread(new QThread())
{
    m_pConnRtuObj = pRtuObj;
    QString *pstrCommType = &(pRtuObj->m_strCommType);
    QStringList *pstrListSerialPara =&(pRtuObj->m_sListSerialPara);
    QStringList *pstrListNetPara =&(pRtuObj->m_sListNetPara);

     qDebug()<< "1:"<< this->thread()<<"2"<<m_pConnThread->thread();
    this->moveToThread(m_pConnThread);
     m_pConnThread->start();
     qDebug()<< "11:"<< this->thread()<<"21"<<m_pConnThread->thread();
    m_chCommuStatus = 0;//赋初值
    m_Serial = NULL;
    m_TcpClient = NULL;
    m_TcpServer = NULL;
    m_CommSettings.m_strCommType = *pstrCommType;
    m_CommSettings.m_sListSerialPara = *pstrListSerialPara;
    m_CommSettings.m_sListNetPara = *pstrListNetPara;
    //翻译本地通信参数
    if(*pstrCommType == "Serial")
    {
       m_CommSettings.name = pstrListSerialPara->at(0);
       m_CommSettings.baudRate = pstrListSerialPara->at(1).toInt();
       if(pstrListSerialPara->at(2) == "N" )
           m_CommSettings.parity = static_cast<QSerialPort::Parity>(0);
       else if(pstrListSerialPara->at(2) == "E" )
           m_CommSettings.parity = static_cast<QSerialPort::Parity>(2);
       else if(pstrListSerialPara->at(2) == "O" )
           m_CommSettings.parity = static_cast<QSerialPort::Parity>(3);
       else
           m_CommSettings.parity = static_cast<QSerialPort::Parity>(0);

      m_CommSettings.dataBits = static_cast<QSerialPort::DataBits>(pstrListSerialPara->at(3).toInt());
      m_CommSettings.stopBits = static_cast<QSerialPort::StopBits>(pstrListSerialPara->at(4).toInt());
    }
    else
    {
        m_CommSettings.txtIP = pstrListNetPara->at(0);
        m_CommSettings.txtPort = pstrListNetPara->at(1);
    }
    connect(this, &MyConnect::sigSetCOM, this, &MyConnect::SlotOpenCommu);
    connect(this, &MyConnect::signCloseCom, this, &MyConnect::SlotCloseCommu);
}

//通信连接
void MyConnect::SlotOpenCommu()
{
    if(m_CommSettings.m_strCommType == "Serial")
    {
        openSerialPort();
    }
    else
    {
        openNet();
    }
}

void MyConnect::openCommu()
{
    qDebug()<< "101:"<< this->thread();
    emit sigSetCOM();
}

//断开通信
void MyConnect::SlotCloseCommu()
{
    if(m_CommSettings.m_strCommType == "Serial")
    {
        closeSerialPort();
    }
    else
    {
        closeNet();
    }
}
//通信断开
void MyConnect::closeCommu()
{
    emit signCloseCom();
}
//打开串口
void MyConnect::openSerialPort()
{
   if(m_Serial != NULL)
   {
       delete m_Serial;
       m_Serial = NULL;
   }
    m_Serial = new QSerialPort;
    m_Serial->moveToThread(m_pConnThread);
    qDebug() << "m_Serial:"<< m_Serial->thread()<<"this"<<this->thread();
   connect(m_Serial, SIGNAL(readyRead()), this, SLOT(readData()));

    m_Serial->setPortName(m_CommSettings.name);
    m_Serial->setBaudRate(m_CommSettings.baudRate);
    m_Serial->setDataBits(m_CommSettings.dataBits);
    m_Serial->setParity(m_CommSettings.parity);
    m_Serial->setStopBits(m_CommSettings.stopBits);
    if (m_Serial->open(QIODevice::ReadWrite))
    {
        SendEvent(tr("Connected to %1").arg(m_CommSettings.m_sListSerialPara.join(",")));
        m_chCommuStatus = 1;
    }
    else
    {
        SendEvent(tr("Open Serial Error, %1").arg(m_CommSettings.m_sListSerialPara.join(",")));
        m_chCommuStatus = 0;
    }
}

//关闭串口
void MyConnect::closeSerialPort()
{
    if (m_Serial->isOpen())
        m_Serial->close();
    m_chCommuStatus = 0;
    SendEvent(tr("%1 Disconnected").arg(m_CommSettings.m_sListSerialPara.at(0)));
}




//TCP client init
void MyConnect::TcpCliInit()
{
    if(m_TcpClient != NULL)
    {
        delete m_TcpClient;
        m_TcpClient = NULL;
    }
    m_TcpClient=new QTcpSocket(this);
    m_TcpClient->moveToThread(m_pConnThread);

    m_TcpClient->abort();//取消原有连接
    connect(m_TcpClient,SIGNAL(readyRead()),this,SLOT(TcpCliReadData()));
    connect(m_TcpClient,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(TcpCliReadError(QAbstractSocket::SocketError)));
}

//TCP server init
void MyConnect::TcpServerInit()
{
    if(m_TcpServer != NULL)
    {
        delete m_TcpServer;
        m_TcpServer = NULL;
    }
    m_TcpServer=new myTcpServer(this);
    m_TcpServer->moveToThread(m_pConnThread);
    connect(m_TcpServer,SIGNAL(ClientConnect(int,QString,int)),this,SLOT(ClientConnect(int,QString,int)));
    connect(m_TcpServer,SIGNAL(ClientDisConnect(int,QString,int)),this,SLOT(ClientDisConnect(int,QString,int)));
    connect(m_TcpServer,SIGNAL(ClientReadData(int,QString,int,QByteArray)),this,SLOT(ClientReadData(int,QString,int,QByteArray)));
}

void MyConnect::openNet()
{
    if(m_CommSettings.m_strCommType == "TcpSever")
    {
        TcpServerInit();
        bool ok=m_TcpServer->listen(QHostAddress::Any,m_CommSettings.txtPort.toInt());////QHostAddress::LocalHost
        if (ok)
        {
            m_chCommuStatus = 1;
            SendEvent(tr("listen in %1,%2").arg(m_CommSettings.txtIP).arg(m_CommSettings.txtPort));
        }
        else
        {
            m_chCommuStatus = 0;
            SendEvent(tr("%1 Listened error,%2").arg(m_CommSettings.txtIP).arg(m_TcpServer->errorString()));

        }
    }
    else if(m_CommSettings.m_strCommType == "TcpClient")
    {
        TcpCliInit();
        m_TcpClient->connectToHost(m_CommSettings.txtIP,m_CommSettings.txtPort.toInt());
        if (m_TcpClient->waitForConnected(1000))
        {
            m_chCommuStatus = 1;
            SendEvent(tr("Connect to %1 : %2").arg(m_CommSettings.txtIP)
                      .arg(m_CommSettings.txtPort));
        }
        else
        {
            m_chCommuStatus = 0;
            SendEvent(tr("Connect %1 error,%2").arg(m_CommSettings.txtIP).arg(m_TcpClient->errorString()));
        }
    }
}
void MyConnect::closeNet()
{
    if(m_CommSettings.m_strCommType == "TcpClient")
    {
        m_TcpClient->disconnectFromHost();
        m_TcpClient->waitForDisconnected(1000);
        if (m_TcpClient->state() == QAbstractSocket::UnconnectedState || m_TcpClient->waitForDisconnected(1000))
        {

            SendEvent(tr("%1 Disconnected").arg(m_CommSettings.txtIP));
            m_chCommuStatus = 0;
        }
        else
        {
            SendEvent(tr("%1 Disconnected Error, %2").arg(m_CommSettings.txtIP).arg(m_TcpClient->errorString()));
            m_chCommuStatus = 0;
        }
    }
    else
    {//服务器端
        m_TcpServer->CloseServer();
        SendEvent(tr("%1 Disconnected").arg(m_CommSettings.txtIP));
        m_chCommuStatus = 0;
    }



}

//服务器端断开连接
void MyConnect::TcpCliReadError(QAbstractSocket::SocketError)
{
    m_TcpClient->disconnectFromHost();
   // SendEvent(tr("%1 连接服务器失败,原因:%1").arg(m_CommSettings.txtIP).arg(m_TcpClient->errorString()));
    m_chCommuStatus = 0;

}
//串口读数据
void MyConnect::readData()
{
    QByteArray buffer = m_Serial->readAll();
    ShowRxTxData(buffer);
    m_RxDaList.append(buffer);

}
//tcp server 读数据
void MyConnect::ClientReadData(int clientID,QString IP,int Port,QByteArray data)
{
    if (!data.isEmpty())
    {
        ShowRxTxData(data);
        m_RxDaList.append(data);
    }
    //qDebug() << tr("当前线程ID：")<< QThread::currentThreadId();
}
//tcp client 读数据
void MyConnect::TcpCliReadData()
{
    QByteArray buffer=m_TcpClient->readAll();
    qDebug() <<"this 1"<<this->thread();
    if (!buffer.isEmpty())
    {
       ShowRxTxData(buffer);
       m_RxDaList.append(buffer);
    }
    //qDebug() << tr("当前线程ID：")<< QThread::currentThreadId();
}




void MyConnect::ClientConnect(int clientID,QString IP,int Port)
{
    SendEvent(tr("客户端:[clientID:%1 IP:%2 Port:%3]上线")
              .arg(clientID).arg(IP).arg(Port));

}

void MyConnect::ClientDisConnect(int clientID,QString IP,int Port)
{
    SendEvent(tr("客户端:[clientID:%1 IP:%2 Port:%3]下线")
              .arg(clientID).arg(IP).arg(Port));
}
//写数据
void MyConnect::writeData(const QByteArray &data)
{

    QString RtuName = m_pConnRtuObj->m_strRtuName;
    if(m_chCommuStatus == 0)
    {
        emit signSendData(data,0x81,RtuName);
        return;
    }
    else
        emit signSendData(data,1,RtuName);
    if(m_CommSettings.m_strCommType == "Serial")
        m_Serial->write(data);
    else if(m_CommSettings.m_strCommType == "TcpClient")
            m_TcpClient->write(data);
    else if(m_CommSettings.m_strCommType == "TcpSever")//服务器
            m_TcpServer->SendData(data);

}



//串口错误处理
void MyConnect::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        SendEvent(tr("Critical Error %1").arg(m_Serial->errorString()));
        closeSerialPort();
    }
}
void MyConnect::SendEvent(QString StrDesc)
{
    MyEvent *pEvent = new MyEvent(QEvent::Type(MSG_EVENT));
    if(m_pConnRtuObj != NULL) pEvent->m_SmsgUnit.m_strRTUName = m_pConnRtuObj->m_strRtuName;
    pEvent->m_SmsgUnit.m_strMsg = StrDesc;
    qApp->postEvent( (QObject*)g_pManagerThread, pEvent);
}
void MyConnect::ShowRxTxData(QByteArray requestData,char TxOrRx)
{
    qDebug() <<"this"<<this->thread();
    QString RtuName = m_pConnRtuObj->m_strRtuName;
    emit signRcvData(requestData,TxOrRx,RtuName);
}

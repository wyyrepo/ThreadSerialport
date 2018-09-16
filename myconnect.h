#ifndef MYCONNECT_H
#define MYCONNECT_H
#include "mytcpserver.h"
#include "myevent.h"
#include <QtSerialPort/QSerialPort>
#include "crtuobject.h"
#include "qmythread.h"

class QMyThread;
class CRTUObject;
class MyConnect : public QObject
{
    Q_OBJECT
public:
    explicit MyConnect(CRTUObject *pRtuObj);

public:
    struct CommSettings {
        QString name;
        qint32 baudRate;
        QSerialPort::DataBits dataBits;
        QSerialPort::Parity parity;
        QSerialPort::StopBits stopBits;
        QString txtIP;
        QString txtPort;
        QString m_strCommType;//通信类型 serial TcpClient TcpServer
        QStringList m_sListSerialPara;//串口参数 COM1:9600:N:8:1
        QStringList m_sListNetPara;//网口参数 192.168.1.1:2404
    };
     CommSettings m_CommSettings;
     char m_chCommuStatus;//通信状态
     CRTUObject *m_pConnRtuObj;
     QList<QByteArray> m_RxDaList;

protected:
     QThread * m_pConnThread;
public:
     QSerialPort *m_Serial; //串口对象
     QTcpSocket *m_TcpClient;//tcp client对象
     myTcpServer *m_TcpServer;//tcp server对象
public:
    void TcpCliInit();
    void TcpServerInit();
    void openNet();
    void closeNet();
    void closeCommu();//断开通信
    void openSerialPort();
    void closeSerialPort();
    void SendEvent(QString StrDesc);
    void ShowRxTxData(QByteArray requestData,char TxOrRx = 0);

signals:
    void signRcvData(const QByteArray &data,char TxOrRx,QString RtuName);
    void signSendData(const QByteArray &data,char TxOrRx,QString RtuName);
    void sigSetCOM();
    void signCloseCom();
public slots:
    void writeData(const QByteArray &data);//写数据
    void SlotOpenCommu();
    void SlotCloseCommu();
    void openCommu();//打开通信或重新打开通信
    void TcpCliReadData();
    void TcpCliReadError(QAbstractSocket::SocketError);

    void readData();//读数据
    void handleError(QSerialPort::SerialPortError error);

    void ClientReadData(int clientID,QString IP,int Port,QByteArray data);
    void ClientConnect(int clientID,QString IP,int Port);
    void ClientDisConnect(int clientID,QString IP,int Port);
};
extern QMap< int , MyConnect* > g_ConnMap;
#endif // MYCONNECT_H

//By realfan   2015.1
#include "myserialport.h"
#include <QThread>
#include <QSerialPort>
#ifdef QT_DEBUG
#include <QDebug>
#include <QDateTime>
#endif
MySerialPort::MySerialPort(const QString strComName)
    : m_strComName(strComName),
      m_pThread(new QThread()),
      m_pCom(new QSerialPort()),
      m_iLen(-1), m_bOpen(false),
      m_bEchoRegExp(false), m_bEchoFlag(false)
{
    m_pCom->moveToThread(m_pThread);
    this->moveToThread(m_pThread);
    m_pThread->start();
    connect(m_pCom, &QSerialPort::readyRead, this, &MySerialPort::slotDataReady, Qt::DirectConnection);

    connect(this, &MySerialPort::sigSetCOM, this, &MySerialPort::slotSetCOM);
    connect(this, &MySerialPort::sigClose, this, &MySerialPort::slotClose);
    connect(this, &MySerialPort::sigClear, this, &MySerialPort::slotClear);
    connect(this, &MySerialPort::sigWrite, this, &MySerialPort::slotWrite);
}
MySerialPort::~MySerialPort()
{
#ifdef QT_DEBUG
    qDebug() << "~MySerialPort:" << QThread::currentThreadId() << "\n";
#endif
    close();
    m_pThread->quit();
    m_pThread->wait();
    delete m_pCom;
    m_pCom = 0;
    delete m_pThread;
    m_pThread = 0;
}
bool MySerialPort::isOpen() const
{
    return m_bOpen;
}
bool MySerialPort::setCOM(const QString strCOM /*=""*/,
                          const int iBautRate /*=9600*/,
                          const int iDataBits /*=8*/,
                          const char chParity /*='N'*/,
                          const char chStopBits /*=1*/)
{
    m_lockSetCOM.lock();
    const int nAvlb = m_semSetCOM.available();
    if(nAvlb > 0)
    {
        m_semSetCOM.tryAcquire(nAvlb);
    }
    emit sigSetCOM(strCOM, iBautRate, iDataBits, chParity, chStopBits);
    const bool bWait = m_semSetCOM.tryAcquire(1, 5000);// bool bWait = m_waitSetCOM.wait(&m_lockSetCOM, 5000);
    m_lockSetCOM.unlock();
    return bWait ? m_bOpen : false;
}
qint64 MySerialPort::write(const QByteArray &byteArray)
{
    return write(byteArray.data(), byteArray.size());
}

qint64 MySerialPort::write(const char *data, qint64 maxSize/* = -1*/)
{
    if(!m_bOpen) return -1;
    m_lockWrite.lock();
    const int nAvlb = m_semWrite.available();
    if(nAvlb > 0)
    {
        m_semWrite.tryAcquire(nAvlb);
    }
    emit sigWrite(data, maxSize);
    const bool bWait = m_semWrite.tryAcquire(1, 5000);
    m_lockWrite.unlock();
    QMutexLocker lk(&m_lockWriteLen);
    qint64 iRet = bWait ? m_iLen : 0;
    return iRet;
}
void MySerialPort::close()
{
    m_lockClose.lock();
    const int nAvlb = m_semClose.available();
    if(nAvlb > 0)
    {
        m_semClose.tryAcquire(nAvlb);
    }
    emit sigClose();
    m_semClose.tryAcquire(1, 5000);
    m_lockClose.unlock();
}
void MySerialPort::clear()
{
    m_lockClear.lock();
    const int nAvlb = m_semClear.available();
    if(nAvlb > 0)
    {
        m_semClear.tryAcquire(nAvlb);
    }
    emit sigClear();
    m_semClear.tryAcquire(1, 5000);// m_waitClear.wait(&m_lockClear, 5000);
    m_lockClear.unlock();
}

bool MySerialPort::EchoCommand(const QString &strCmd, const QString &strExpectReply /* ="" */, QString *pstrReply /* =0 */)
{
    QMutexLocker lk(&m_lockEcho);
    m_bEchoRegExp = false;
    m_strExpectReply = strExpectReply;
    clear();
    setEchoFlag(true);
    const int nAvlb = m_semEcho.available();
    if(nAvlb > 0)
    {
        m_semEcho.tryAcquire(nAvlb);
    }
    const QByteArray ba = (strCmd + "\r\n").toLatin1();
    const int iLen = ba.length();
    if(write(ba) < iLen)
    {
        setEchoFlag(false);
        return false;
    }
    bool bRet = m_semEcho.tryAcquire(1, 10000);// bool bRet = m_waitEcho.wait(&m_lockEcho, 10000);
    //qDebug() << QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") << "write " << strCmd << " OK\n";
    if("" == strExpectReply && 0 == pstrReply)
    {
        setEchoFlag(false);
        return true;
    }
    if(pstrReply)
    {
        pstrReply->clear();
    }
#ifdef QT_DEBUG
    qDebug() << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << "EchoCommand wait " << bRet << " " << m_strReply << " |\n";
#endif
    setEchoFlag(false);
    if(false == bRet)
    {
        if(m_strReply.indexOf(strExpectReply) >= 0 && m_strReply.endsWith('\n'))
        {
            bRet = true;
        }
    }
    else if(m_strReply.indexOf("ES") >= 0 || m_strReply.indexOf("EL") >= 0)
    {
        bRet = false;
    }
    if(pstrReply)
    {
        *pstrReply = m_strReply;
    }

    return bRet;
}
//====================================================


void MySerialPort::slotWrite(const char *pch, qint64 maxSize)
{
    //qDebug() << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " write begin\n";
    m_lockWriteLen.lock();
    m_iLen = -1;
    if(m_pCom->isOpen())
    {
        m_iLen = (maxSize < 0) ? m_pCom->write(pch) : m_pCom->write(pch, maxSize);
    }
    m_lockWriteLen.unlock();
    m_semWrite.release();
    //qDebug() << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " write OK\n";
}

void MySerialPort::slotSetCOM(const QString strCOM, const int iBautRate, const int iDataBits, const char chParity, const char chStopBits)
{
    m_pCom->close();
    m_bOpen = false;
    if(strCOM.length() > 0)
    {
        m_strComName = strCOM;
    }
    if(m_strComName.length() < 1)
    {
        m_semSetCOM.release();// m_waitSetCOM.wakeAll();
        return;
    }
    m_pCom->setPortName(m_strComName);

    if(false == m_pCom->isOpen())
    {
        if(false == m_pCom->open(QIODevice::ReadWrite))
        {
            m_semSetCOM.release();// m_waitSetCOM.wakeAll();
        }
    }
    QSerialPort::BaudRate eBaudRate = QSerialPort::Baud9600;
    switch(iBautRate)
    {
    case 115200:
        eBaudRate = QSerialPort::Baud115200;
        break;
    case 9600:
        eBaudRate = QSerialPort::Baud9600;
        break;
    case 2400:
        eBaudRate = QSerialPort::Baud2400;
        break;
    case 1200:
        eBaudRate = QSerialPort::Baud1200;
        break;
    case 4800:
        eBaudRate = QSerialPort::Baud4800;
        break;
    case 19200:
        eBaudRate = QSerialPort::Baud19200;
        break;
    case 38400:
        eBaudRate = QSerialPort::Baud38400;
        break;
    case 57600:
        eBaudRate = QSerialPort::Baud57600;
        break;
    default:
        //eBaudRate = QSerialPort::UnknownBaud;
        break;
    }
    m_pCom->setBaudRate(eBaudRate);

    QSerialPort::DataBits eDataBits = QSerialPort::Data8;
    switch(iDataBits)
    {
    case 8:
        eDataBits = QSerialPort::Data8;
        break;
    case 7:
        eDataBits = QSerialPort::Data7;
        break;
    case 6:
        eDataBits = QSerialPort::Data6;
        break;
    case 5:
        eDataBits = QSerialPort::Data5;
        break;
    default:
        //eDataBits = QSerialPort::UnknownDataBits;
        break;
    }
    m_pCom->setDataBits(eDataBits);

    QSerialPort::Parity eParity = QSerialPort::NoParity;
    switch(chParity)
    {
    case 'N':
        eParity = QSerialPort::NoParity;
        break;
    case 'E':
        eParity = QSerialPort::EvenParity;
        break;
    case 'O':
        eParity = QSerialPort::OddParity;
        break;
    case 'S':
        eParity = QSerialPort::SpaceParity;
        break;
    case 'M':
        eParity = QSerialPort::MarkParity;
        break;
    default:
        //eParity = QSerialPort::UnknownParity;
        break;
    }
    m_pCom->setParity(eParity);

    QSerialPort::StopBits eStopBits = QSerialPort::OneStop;
    switch(chStopBits)
    {
    case 1:
        eStopBits = QSerialPort::OneStop;
        break;
    case 2:
        eStopBits = QSerialPort::TwoStop;
        break;
    case 3:
        eStopBits = QSerialPort::OneAndHalfStop;
        break;
    default:
        //eStopBits = QSerialPort::UnknownStopBits;
        break;
    }
    m_pCom->setStopBits(eStopBits);

    /*
        enum FlowControl {
            NoFlowControl,
            HardwareControl,
            SoftwareControl,
            UnknownFlowControl = -1
        }; */

    m_pCom->clear();
    m_bOpen = true;
    m_semSetCOM.release();
}
void MySerialPort::slotClose()
{
    //qDebug() << "slotClose:" << QThread::currentThreadId() <<"\n";
    m_pCom->close();
    m_bOpen = false;
    m_semClose.release();
}
void MySerialPort::slotClear()
{
    m_pCom->clear();
    m_lockInBuffer.lock();
    m_strInBuffer.clear();
    m_strReply.clear();
    m_lockInBuffer.unlock();
    m_semClear.release();
}
void MySerialPort::slotDataReady()
{
    QSerialPort *const pCom = m_pCom;
    if(getEchoFlag())
    {
        QMutexLocker lk(&m_lockInBuffer);
        m_strInBuffer.push_back(pCom->readAll());
        m_strReply = m_strInBuffer;
        if(m_bEchoRegExp)
        {
            if(m_strInBuffer.indexOf(m_rxExpectReply) >= 0)
            {
                setEchoFlag(false);
                m_semEcho.release();
            }
        }
        else
        {
            int idx = m_strInBuffer.indexOf(m_strExpectReply);
            if(idx >= 0 && m_strInBuffer.indexOf('\n', idx + 1) >= 0)
            {
                setEchoFlag(false);
#ifdef QT_DEBUG
                qDebug() << QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") << m_strReply << " wakeAll\n";
#endif
                m_semEcho.release();
                return;
            }
        }
    }
    else
    {
        emit sigDataReady(pCom->readAll());
    }
}

#if 0
bool MySerialPort::EchoCommand(const QString &strCmd, const QRegExp &rx /* ="" */, QString *pstrReply /* =0 */)
{
    m_strReply = "";
    m_bEchoRegExp = true;
    m_rxExpectReply = rx;
    m_lockInBuffer.lock();
    m_strInBuffer.clear();
    m_lockInBuffer.unlock();
    m_bEchoFlag = true;
    if(write((strCmd + "\r\n").toLatin1()) < strCmd.length() + 2)
    {
        return false;
    }

    QMutexLocker lk(&m_lockEcho); //.lock();
    bool bRet = (m_waitEcho.wait(&m_lockEcho, 5000));
    m_bEchoFlag = false;
    m_bEchoRegExp = false;
    if(pstrReply)
    {
        *pstrReply = bRet ? m_strReply : "";
    }

    return bRet;
}
#endif
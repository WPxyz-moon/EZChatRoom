#include "serverconnection.h"
#include <QTcpSocket>
#include "codec_pack.h"
#include <vector>

ServerConnection::ServerConnection(QObject *parent)
    : QObject{parent},m_socket(new QTcpSocket(this))
{
    connect(m_socket,&QTcpSocket::connected,this,&ServerConnection::onConnected);
    connect(m_socket,&QTcpSocket::readyRead,this,&ServerConnection::onReadyRead);
    connect(m_socket,&QTcpSocket::errorOccurred,this,&ServerConnection::onError);
    connect(m_socket,&QTcpSocket::bytesWritten,this,&ServerConnection::bytesWritten);

    // 用 stateChanged 统一判断“从已连接变成未连接”，
    // 这样主动 abort、服务器关闭、网络断开都只会上报一次 disconnected。
    connect(m_socket,&QAbstractSocket::stateChanged,this,
            [this](QAbstractSocket::SocketState state){
        if(state == QAbstractSocket::UnconnectedState && m_wasConnected){
            m_wasConnected = false;
            emit disconnected();
        }
    });
}

void ServerConnection::send(const Package& msg)
{
    std::string str = codec::serialize(msg);
    m_socket->write(str.data(),str.size());
}

void ServerConnection::connectToHost(const QString& ip,quint16 port)
{
    m_rx.clear();
    m_socket->abort();
    m_socket->connectToHost(ip,port);
}

void ServerConnection::disconnectFromHost()
{
    m_rx.clear();

    const QAbstractSocket::SocketState state = m_socket->state();
    if(state == QAbstractSocket::UnconnectedState){
        emit disconnected();
        return;
    }

    const bool wasConnected = m_wasConnected;

    // abort 会丢弃未发出的数据，满足“再次按下立即中断”的要求
    m_socket->abort();

    // 曾经连上过时，abort 会走 stateChanged -> 统一上报 disconnected；
    // 还在连接/解析主机时不会，这里补一次。
    if(!wasConnected){
        emit disconnected();
    }
}

bool ServerConnection::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool ServerConnection::isConnecting() const
{
    const QAbstractSocket::SocketState state = m_socket->state();
    return state == QAbstractSocket::HostLookupState
        || state == QAbstractSocket::ConnectingState;
}

qint64 ServerConnection::bufferedBytes() const
{
    return m_socket->bytesToWrite();
}

void ServerConnection::onReadyRead()
{
    std::vector<Package> pkgs;
    QByteArray qarray(m_socket->readAll());
    m_rx.append(qarray.constData(),qarray.size());
    if(!codec::parse(m_rx,pkgs)){
        m_rx.clear();
        m_socket->abort();
        emit protocolError(QStringLiteral("bad frame"));
        return;
    }
    for(const Package& pkg : pkgs){
        emit frameReceived(pkg);
    }

}

void ServerConnection::onConnected()
{
    qDebug() << "Connect Server Sucessfully";
    m_wasConnected = true;
    emit connected();
}

void ServerConnection::onError()
{
    qDebug() << "ServerConnection error!";
    emit socketError(m_socket->errorString());
}
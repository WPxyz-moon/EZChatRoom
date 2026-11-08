#include "serverconnection.h"
#include <QTcpSocket>
#include "codec_pack.h"
#include <vector>

ServerConnection::ServerConnection(QObject *parent)
    : QObject{parent},m_socket(new QTcpSocket(this))
{
    connect(m_socket,&QTcpSocket::connected,this,&ServerConnection::onConnected);
    connect(m_socket,&QTcpSocket::disconnected,this,&ServerConnection::onDisConnected);
    connect(m_socket,&QTcpSocket::readyRead,this,&ServerConnection::onReadyRead);
    connect(m_socket,&QTcpSocket::errorOccurred,this,&ServerConnection::onError);
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

bool ServerConnection::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
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
    emit connected();
}

void ServerConnection::onDisConnected()
{
    qDebug() << "Server disconnected";
    emit disconnected();
}

void ServerConnection::onError()
{
    qDebug() << "ServerConnection error!";
    emit socketError(m_socket->errorString());
}
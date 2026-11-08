#include "clientcore.h"
#include "serverconnection.h"
#include "codec.h"
#include "Messages.h"

#include <QDebug>

ClientCore::ClientCore(QObject *parent):
    QObject(parent),
    m_conn(new ServerConnection(this))
{
    connect(m_conn,&ServerConnection::connected,this,&ClientCore::onConnected);
    connect(m_conn,&ServerConnection::frameReceived,this,&ClientCore::onFrame);
    connect(m_conn,&ServerConnection::disconnected,this,[this]{
        m_loggedIn = false;
        emit disconnected();
    });

    connect(m_conn,&ServerConnection::protocolError,this,&ClientCore::protocolError);
    connect(m_conn,&ServerConnection::socketError,this,&ClientCore::connectionError);
}

void ClientCore::connectServer(const QString &host, quint16 port)
{
    m_loggedIn = false;
    m_conn->connectToHost(host,port);
}

void ClientCore::login(const QString &name)
{
    if(name.isEmpty()) return;
    if(m_loggedIn){
        emit repeatLogin(name);
        return;
    }

    if(!m_conn->isConnected()){
        emit connectionError(QStringLiteral("not connected"));
        return;
    }

    m_name = name;

    LoginReq req;
    req.name = name.toStdString();
    m_conn->send(Package(PackType::LOGIN,codec::encodeLogin(req)));
}

void ClientCore::sendChat(const QString &text)
{
    if(!m_loggedIn) return;

    Message msg;
    msg.time = 0;            // C->S 留 0，由服务器填
    msg.cliName.clear();     // 空 = 广播
    msg.text = text.toStdString();
    m_conn->send(Package(PackType::MESSAGE,codec::encodeMessage(msg)));
}

void ClientCore::sendChatTo(const QString &target, const QString &text)
{
    if(!m_loggedIn) return;

    Message msg;
    msg.time = 0;
    msg.cliName = target.toStdString();
    msg.text = text.toStdString();
    m_conn->send(Package(PackType::MESSAGE,codec::encodeMessage(msg)));
}

void ClientCore::onFrame(const Package &frame)
{
    switch(frame.type_){
    case PackType::MESSAGE :{
        Message msg{0};
        if(!codec::decodeMessage(frame.body_,msg)){
            emit protocolError(QStringLiteral("bad MESSAGE body"));
            return;
        }

        const QDateTime time = msg.time
            ? QDateTime::fromSecsSinceEpoch(static_cast<qint64>(msg.time))
            : QDateTime::currentDateTime();

        emit messageReceived(
            time,
            QString::fromStdString(msg.cliName),
            QString::fromStdString(msg.text)
        );
        break;
    }
    case PackType::LOGIN_ACK :{
        LoginAck ack{0};
        if (!codec::decodeLoginAck(frame.body_,ack)){
            emit protocolError(QStringLiteral("bad LOGIN_ACK body"));
            return;
        }

        const bool ok = (ack.code == 0);
        m_loggedIn = ok;

        emit loginResult(
            ok,
            QString::fromStdString(ack.reason)
        );

        if(!ok){
            m_name.clear();
            emit serverError(
                ack.code,
                QString::fromStdString(ack.reason)
            );
        }
        break;
    }
    case PackType::ERROR_MSG :{
        ErrorMsg err{};
        if(!codec::decodeErrorMsg(frame.body_,err)){
            emit protocolError(QStringLiteral("bad ERROR_MSG body"));
            return;
        }

        emit serverError(
            err.code,
            QString::fromStdString(err.reason)
        );
        break;
    }
    default:
        qDebug() << "unhandled packet type:" << int(frame.type_);
        break;
    }
}

void ClientCore::onConnected()
{
    m_loggedIn = false;
    emit connected();
}

#ifndef SERVERCONNECTION_H
#define SERVERCONNECTION_H

#include <QObject>
#include "Package.h"

class QTcpSocket;

//协议帧处理实现包的分发，并缓存半包
class ServerConnection : public QObject
{
    Q_OBJECT
public:
    explicit ServerConnection(QObject *parent = nullptr);

    void send(const Package& msg);

    void connectToHost(const QString& ip,quint16 port);

    // 主动断开：正在连接/已连接都立即 abort，并保证上层收到一次 disconnected
    void disconnectFromHost();

    bool isConnected() const;
    bool isConnecting() const;

    // 发送缓冲里还没写出去的字节数（上传时做背压用）
    qint64 bufferedBytes() const;

signals:

    void frameReceived(const Package& pkgs);

    void protocolError(const QString& reason);

    void socketError(const QString& reason);

    void connected();

    void disconnected();

    // 直通 QTcpSocket::bytesWritten，上传泵用它继续灌数据
    void bytesWritten(qint64 bytes);

private slots:

    void onReadyRead();
    void onConnected();
    void onError();

private:
    std::string m_rx;
    QTcpSocket* m_socket;
    bool m_wasConnected = false;
};

#endif // SERVERCONNECTION_H

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

    bool isConnected() const;

signals:

    void frameReceived(const Package& pkgs);

    void protocolError(const QString& reason);

    void socketError(const QString& reason);

    void connected();

    void disconnected();

private slots:

    void onReadyRead();
    void onConnected();
    void onDisConnected();
    void onError();

private:
    std::string m_rx;
    QTcpSocket* m_socket;
};

#endif // SERVERCONNECTION_H

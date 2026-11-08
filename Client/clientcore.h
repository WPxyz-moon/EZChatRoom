#ifndef CLIENTCORE_H
#define CLIENTCORE_H

#include <QObject>
#include <QDateTime>

class ServerConnection;
struct Package;

//负责把帧转为Message等类然后处理，然后给上层返回QString，上层只用QString即可沟通
class ClientCore : public QObject
{
    Q_OBJECT
public:

    explicit ClientCore(QObject *parent = nullptr);

    void connectServer(const QString& host,quint16 port);

    void login(const QString& name);

    void sendChat(const QString& text);

    void sendChatTo(const QString& target,const QString& text);

    bool isLoggedIn() const { return m_loggedIn; }

    QString name() const { return m_loggedIn ? m_name : QString(); }

signals:

    void connected();
    void disconnected();
    void loginResult(bool ok,const QString& reason);
    void messageReceived(const QDateTime& time,const QString& from,const QString& msg);

    void protocolError(const QString& reason);
    void connectionError(const QString& reason);
    void serverError(quint16 code,const QString& reason);
    void repeatLogin(const QString& name);

private slots:

    void onFrame(const Package& frame);

    void onConnected();

private:

    ServerConnection* m_conn = nullptr;
    QString m_name;
    bool m_loggedIn = false;
};

#endif // CLIENTCORE_H

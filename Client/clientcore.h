#ifndef CLIENTCORE_H
#define CLIENTCORE_H

#include <QObject>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>

#include <memory>

class ServerConnection;
struct Package;

//负责把帧转为Message等类然后处理，然后给上层返回QString，上层只用QString即可沟通
class ClientCore : public QObject
{
    Q_OBJECT
public:

    explicit ClientCore(QObject *parent = nullptr);

    void connectServer(const QString& host,quint16 port);

    // 主动断开（连接中/已连接都可用）
    void disconnectServer();

    bool isConnected() const;
    bool isConnecting() const;

    void login(const QString& name);

    void sendChat(const QString& text);

    void sendChatTo(const QString& target,const QString& text);

    // 上传本地文件；返回 false 表示本地参数不合法或已有传输在进行
    bool sendFile(const QString& path);

    // 下载服务器上的文件（remoteName 可以是文件名或 fileId），保存到 savePath
    bool downloadFile(const QString& remoteName,const QString& savePath);

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

    //文件传输：开始/进度/结束。结束信号里的 reason 在失败时非空
    void fileUploadStarted(const QString& name,quint64 totalBytes);
    void fileUploadProgress(quint64 sentBytes,quint64 totalBytes);
    void fileUploadFinished(const QString& name,bool ok,const QString& reason);

    void fileDownloadStarted(const QString& name,quint64 totalBytes);
    void fileDownloadProgress(quint64 receivedBytes,quint64 totalBytes);
    void fileDownloadFinished(const QString& name,bool ok,const QString& reason,
                              const QString& savePath);

    //本地参数错误/已有传输等，未进入协议流程
    void transferError(const QString& reason);

private slots:

    void onFrame(const Package& frame);

    void onConnected();

private:

    static constexpr quint32 kChunkSize = 64 * 1024;
    static constexpr quint64 kMaxFileSize = 512ull * 1024 * 1024;
    static constexpr qint64 kHighWater = 1024 * 1024;

    struct UploadState {
        bool active = false;
        bool waitingAck = false;
        bool doneSent = false;
        bool pumping = false;
        std::unique_ptr<QFile> file;
        QString name;
        quint32 fileId = 0;
        quint32 totalChunks = 0;
        quint32 nextChunk = 0;
        quint64 totalSize = 0;
        quint64 sent = 0;
    };

    struct DownloadState {
        bool active = false;
        bool waitingMeta = false;
        std::unique_ptr<QSaveFile> file;
        QString name;
        QString savePath;
        quint32 fileId = 0;
        quint32 totalChunks = 0;
        quint32 nextChunk = 0;
        quint64 totalSize = 0;
        quint64 received = 0;
        QByteArray expectedMd5;
        QCryptographicHash hash{QCryptographicHash::Md5};
    };

    void pumpUpload();
    void finishUpload(bool ok,const QString& reason);
    void resetUpload();

    void finishDownload(bool ok,const QString& reason);
    void resetDownload();

    ServerConnection* m_conn = nullptr;
    QString m_name;
    bool m_loggedIn = false;

    UploadState m_upload;
    DownloadState m_download;
};

#endif // CLIENTCORE_H

#include "clientcore.h"
#include "serverconnection.h"
#include "codec.h"
#include "Messages.h"
#include "FileMessages.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <QSaveFile>

ClientCore::ClientCore(QObject *parent):
    QObject(parent),
    m_conn(new ServerConnection(this))
{
    connect(m_conn,&ServerConnection::connected,this,&ClientCore::onConnected);
    connect(m_conn,&ServerConnection::frameReceived,this,&ClientCore::onFrame);
    connect(m_conn,&ServerConnection::disconnected,this,[this]{
        const bool requested = m_disconnectRequested;
        m_disconnectRequested = false;

        if(m_upload.active){
            finishUpload(false,QStringLiteral("连接已断开"));
        }
        if(m_download.active){
            finishDownload(false,QStringLiteral("连接已断开"));
        }
        m_loggedIn = false;

        if(requested){
            emit disconnected();
        }else{
            emit connectionLost();
        }
    });

    connect(m_conn,&ServerConnection::protocolError,this,&ClientCore::protocolError);
    connect(m_conn,&ServerConnection::socketError,this,&ClientCore::connectionError);

    // 上传背压：缓冲区腾出空间后继续发块
    connect(m_conn,&ServerConnection::bytesWritten,this,[this](qint64){
        pumpUpload();
    });
}

void ClientCore::connectServer(const QString &host, quint16 port)
{
    if(m_upload.active){
        finishUpload(false,QStringLiteral("重新连接"));
    }
    if(m_download.active){
        finishDownload(false,QStringLiteral("重新连接"));
    }
    m_loggedIn = false;
    m_conn->connectToHost(host,port);
}

void ClientCore::disconnectServer()
{
    // 标记为用户主动断开，避免上层误报“服务器断开”
    m_disconnectRequested = true;
    m_conn->disconnectFromHost();
}

bool ClientCore::isConnected() const
{
    return m_conn->isConnected();
}

bool ClientCore::isConnecting() const
{
    return m_conn->isConnecting();
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
    msg.time = 0;                        // C->S 留 0，由服务器填
    msg.sender = m_name.toStdString();   // 发送者
    msg.text = text.toStdString();
    msg.target.clear();                  // 空 = 广播
    m_conn->send(Package(PackType::MESSAGE,codec::encodeMessage(msg)));
}

void ClientCore::sendChatTo(const QString &target, const QString &text)
{
    if(!m_loggedIn) return;

    Message msg;
    msg.time = 0;
    msg.sender = m_name.toStdString();
    msg.text = text.toStdString();
    msg.target = target.toStdString();   // 非空 = 私聊
    m_conn->send(Package(PackType::MESSAGE,codec::encodeMessage(msg)));
}

bool ClientCore::sendFile(const QString &path)
{
    if(!m_loggedIn){
        emit transferError(QStringLiteral("请先登录"));
        return false;
    }
    if(m_upload.active || m_download.active){
        emit transferError(QStringLiteral("已有文件传输正在进行"));
        return false;
    }

    const QFileInfo info(path);
    if(!info.exists() || !info.isFile()){
        emit transferError(QStringLiteral("文件不存在：%1").arg(path));
        return false;
    }

    const quint64 size = static_cast<quint64>(info.size());
    if(size > kMaxFileSize){
        emit transferError(QStringLiteral("文件超过 512MB 上限"));
        return false;
    }

    auto file = std::make_unique<QFile>(path);
    if(!file->open(QIODevice::ReadOnly)){
        emit transferError(QStringLiteral("无法打开文件：%1").arg(file->errorString()));
        return false;
    }

    // 分块读一遍算 MD5，顺便确认文件真的能完整读出
    QCryptographicHash hash(QCryptographicHash::Md5);
    quint64 hashed = 0;
    while(!file->atEnd()){
        const QByteArray block = file->read(kChunkSize);
        if(block.isEmpty() && file->error() != QFileDevice::NoError){
            emit transferError(QStringLiteral("读取文件失败：%1").arg(file->errorString()));
            return false;
        }
        hash.addData(block);
        hashed += static_cast<quint64>(block.size());
    }
    if(hashed != size || !file->seek(0)){
        emit transferError(QStringLiteral("读取文件失败"));
        return false;
    }

    const quint32 totalChunks =
        static_cast<quint32>((size + kChunkSize - 1) / kChunkSize);

    m_upload.active = true;
    m_upload.waitingAck = true;
    m_upload.doneSent = false;
    m_upload.pumping = false;
    m_upload.file = std::move(file);
    m_upload.name = info.fileName();
    m_upload.fileId = 0;
    m_upload.totalChunks = totalChunks;
    m_upload.nextChunk = 0;
    m_upload.totalSize = size;
    m_upload.sent = 0;

    FileMeta meta;
    meta.fileId = 0;                    // 由服务器分配
    meta.name = m_upload.name.toStdString();
    meta.size = size;
    meta.totalChunks = totalChunks;
    meta.md5 = hash.result().toHex().toStdString();   // 32 位小写 hex

    m_conn->send(Package(PackType::FILE_UPLOAD_META,codec::encodeFileMeta(meta)));

    emit fileUploadStarted(m_upload.name,size);
    emit fileUploadProgress(0,size);
    return true;
}

bool ClientCore::downloadFile(const QString &remoteName, const QString &savePath)
{
    if(!m_loggedIn){
        emit transferError(QStringLiteral("请先登录"));
        return false;
    }
    if(m_upload.active || m_download.active){
        emit transferError(QStringLiteral("已有文件传输正在进行"));
        return false;
    }

    const QString name = remoteName.trimmed();
    if(name.isEmpty() || savePath.isEmpty()){
        emit transferError(QStringLiteral("文件名或保存路径为空"));
        return false;
    }

    auto file = std::make_unique<QSaveFile>(savePath);
    if(!file->open(QIODevice::WriteOnly)){
        emit transferError(QStringLiteral("无法创建本地文件：%1").arg(file->errorString()));
        return false;
    }

    m_download.active = true;
    m_download.waitingMeta = true;
    m_download.file = std::move(file);
    m_download.name = name;
    m_download.savePath = savePath;
    m_download.fileId = 0;
    m_download.totalChunks = 0;
    m_download.nextChunk = 0;
    m_download.totalSize = 0;
    m_download.received = 0;
    m_download.expectedMd5.clear();
    m_download.hash.reset();

    FileDownloadReq req;
    req.name = name.toStdString();
    m_conn->send(Package(PackType::FILE_DOWNLOAD_REQ,codec::encodeFileDownloadReq(req)));
    return true;
}

void ClientCore::requestFileList()
{
    if(!m_loggedIn) return;
    m_conn->send(Package(PackType::FILE_LIST_REQ,std::string()));
}

void ClientCore::pumpUpload()
{
    if(!m_upload.active || m_upload.waitingAck || m_upload.pumping || !m_upload.file){
        return;
    }

    // bytesWritten 有可能在 write 内部同步回调，防重入
    m_upload.pumping = true;

    while(m_upload.active && !m_upload.waitingAck
          && m_upload.nextChunk < m_upload.totalChunks
          && m_conn->bufferedBytes() < kHighWater){
        const QByteArray data = m_upload.file->read(kChunkSize);
        if(data.isEmpty()){
            m_upload.pumping = false;
            finishUpload(false,QStringLiteral("读取文件失败"));
            return;
        }

        FileChunk chunk;
        chunk.fileId = m_upload.fileId;
        chunk.chunkIndex = m_upload.nextChunk;
        chunk.data.assign(data.constData(),static_cast<size_t>(data.size()));
        m_conn->send(Package(PackType::FILE_UPLOAD_CHUNK,codec::encodeFileChunk(chunk)));

        ++m_upload.nextChunk;
        m_upload.sent += static_cast<quint64>(data.size());
        emit fileUploadProgress(m_upload.sent,m_upload.totalSize);
    }

    if(m_upload.active && !m_upload.waitingAck
       && m_upload.nextChunk >= m_upload.totalChunks && !m_upload.doneSent){
        FileUploadDone done;
        done.fileId = m_upload.fileId;
        m_conn->send(Package(PackType::FILE_UPLOAD_DONE,codec::encodeFileUploadDone(done)));
        m_upload.doneSent = true;
    }

    m_upload.pumping = false;
}

void ClientCore::finishUpload(bool ok, const QString &reason)
{
    if(!m_upload.active) return;

    const QString name = m_upload.name;
    resetUpload();
    emit fileUploadFinished(name,ok,reason);
}

void ClientCore::resetUpload()
{
    if(m_upload.file){
        m_upload.file->close();
        m_upload.file.reset();
    }
    m_upload.active = false;
    m_upload.waitingAck = false;
    m_upload.doneSent = false;
    m_upload.pumping = false;
    m_upload.name.clear();
    m_upload.fileId = 0;
    m_upload.totalChunks = 0;
    m_upload.nextChunk = 0;
    m_upload.totalSize = 0;
    m_upload.sent = 0;
}

void ClientCore::finishDownload(bool ok, const QString &reason)
{
    if(!m_download.active) return;

    const QString name = m_download.name;
    const QString savePath = m_download.savePath;
    bool committed = ok;
    QString finalReason = reason;

    if(m_download.file){
        if(ok){
            if(!m_download.file->commit()){
                committed = false;
                finalReason = m_download.file->errorString();
            }
        }else{
            m_download.file->cancelWriting();
        }
        m_download.file.reset();
    }

    resetDownload();
    emit fileDownloadFinished(name,committed,finalReason,savePath);
}

void ClientCore::resetDownload()
{
    if(m_download.file){
        m_download.file->cancelWriting();
        m_download.file.reset();
    }
    m_download.active = false;
    m_download.waitingMeta = false;
    m_download.name.clear();
    m_download.savePath.clear();
    m_download.fileId = 0;
    m_download.totalChunks = 0;
    m_download.nextChunk = 0;
    m_download.totalSize = 0;
    m_download.received = 0;
    m_download.expectedMd5.clear();
    m_download.hash.reset();
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
            QString::fromStdString(msg.sender),
            QString::fromStdString(msg.target),
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

        const QString reason = QString::fromStdString(err.reason);

        // 上传/等待下载元信息阶段出错：本次传输按失败收尾
        if(m_upload.active){
            finishUpload(false,reason);
        }
        if(m_download.active && m_download.waitingMeta){
            finishDownload(false,reason);
        }

        emit serverError(err.code,reason);
        break;
    }

    // ===== 文件上传 =====
    case PackType::FILE_UPLOAD_META_ACK :{
        FileUploadMetaAck ack;
        if(!codec::decodeFileUploadMetaAck(frame.body_,ack)){
            emit protocolError(QStringLiteral("bad FILE_UPLOAD_META_ACK body"));
            return;
        }
        if(!m_upload.active || !m_upload.waitingAck){
            qDebug() << "unexpected FILE_UPLOAD_META_ACK";
            return;
        }

        if(ack.code != 0){
            const QString reason = QString::fromStdString(ack.reason);
            finishUpload(false,reason.isEmpty() ? QStringLiteral("服务器拒绝上传") : reason);
            return;
        }

        m_upload.fileId = ack.fileId;
        m_upload.waitingAck = false;
        if(!m_upload.file || !m_upload.file->seek(0)){
            finishUpload(false,QStringLiteral("无法读取文件"));
            return;
        }
        pumpUpload();
        break;
    }
    case PackType::FILE_UPLOAD_DONE_ACK :{
        FileUploadDoneAck ack;
        if(!codec::decodeFileUploadDoneAck(frame.body_,ack)){
            emit protocolError(QStringLiteral("bad FILE_UPLOAD_DONE_ACK body"));
            return;
        }
        if(!m_upload.active || ack.fileId != m_upload.fileId){
            return;
        }

        if(ack.code == 0){
            finishUpload(true,QString());
        }else{
            const QString reason = QString::fromStdString(ack.reason);
            finishUpload(false,reason.isEmpty() ? QStringLiteral("上传失败") : reason);
        }
        break;
    }

    // ===== 文件下载 =====
    case PackType::FILE_DOWNLOAD_META :{
        FileMeta meta;
        if(!codec::decodeFileMeta(frame.body_,meta)){
            emit protocolError(QStringLiteral("bad FILE_DOWNLOAD_META body"));
            return;
        }
        if(!m_download.active || !m_download.waitingMeta){
            qDebug() << "unexpected FILE_DOWNLOAD_META";
            return;
        }

        m_download.waitingMeta = false;
        m_download.fileId = meta.fileId;
        m_download.name = QString::fromStdString(meta.name);
        m_download.totalSize = meta.size;
        m_download.totalChunks = meta.totalChunks;
        m_download.nextChunk = 0;
        m_download.received = 0;
        m_download.expectedMd5 = QByteArray::fromStdString(meta.md5).toLower();

        emit fileDownloadStarted(m_download.name,m_download.totalSize);
        emit fileDownloadProgress(0,m_download.totalSize);
        break;
    }
    case PackType::FILE_DOWNLOAD_CHUNK :{
        FileChunk chunk;
        if(!codec::decodeFileChunk(frame.body_,chunk)){
            emit protocolError(QStringLiteral("bad FILE_DOWNLOAD_CHUNK body"));
            return;
        }
        if(!m_download.active || m_download.waitingMeta){
            return;
        }
        if(chunk.fileId != m_download.fileId){
            return;
        }
        if(chunk.chunkIndex != m_download.nextChunk){
            finishDownload(false,QStringLiteral("数据块顺序错误"));
            return;
        }

        if(!m_download.file
           || m_download.file->write(chunk.data.data(),
                                     static_cast<qint64>(chunk.data.size()))
                  != static_cast<qint64>(chunk.data.size())){
            finishDownload(false,QStringLiteral("写入本地文件失败"));
            return;
        }

        m_download.hash.addData(
            QByteArrayView(chunk.data.data(), static_cast<qsizetype>(chunk.data.size())));
        ++m_download.nextChunk;
        m_download.received += static_cast<quint64>(chunk.data.size());
        emit fileDownloadProgress(m_download.received,m_download.totalSize);
        break;
    }
    case PackType::FILE_LIST_CHANGED :{
        // 服务器广播：有人上传成功，文件索引变了，自己重新拉一份列表
        requestFileList();
        break;
    }
    case PackType::FILE_LIST_RESP :{
        FileListResp resp;
        if(!codec::decodeFileListResp(frame.body_,resp)){
            emit protocolError(QStringLiteral("bad FILE_LIST_RESP body"));
            return;
        }

        QList<RemoteFileInfo> files;
        files.reserve(static_cast<qsizetype>(resp.files.size()));
        for(const auto& f : resp.files){
            RemoteFileInfo info;
            info.id = f.fileId;
            info.name = QString::fromStdString(f.name);
            info.size = f.size;
            files.append(std::move(info));
        }
        emit fileListReceived(files);
        break;
    }
    case PackType::FILE_DOWNLOAD_DONE :{
        FileDownloadDone done;
        if(!codec::decodeFileDownloadDone(frame.body_,done)){
            emit protocolError(QStringLiteral("bad FILE_DOWNLOAD_DONE body"));
            return;
        }
        if(!m_download.active || m_download.waitingMeta || done.fileId != m_download.fileId){
            return;
        }

        if(m_download.received != m_download.totalSize){
            finishDownload(false,QStringLiteral("文件大小不一致"));
            return;
        }

        if(!m_download.expectedMd5.isEmpty()
           && m_download.hash.result().toHex() != m_download.expectedMd5){
            finishDownload(false,QStringLiteral("MD5 校验失败"));
            return;
        }

        finishDownload(true,QString());
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

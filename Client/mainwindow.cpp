#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "clientcore.h"

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QIntValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>

namespace {

QString formatBytes(quint64 bytes)
{
    constexpr quint64 kb = 1024;
    if (bytes >= kb * kb * kb) {
        return QStringLiteral("%1 GB").arg(bytes / double(kb * kb * kb), 0, 'f', 2);
    }
    if (bytes >= kb * kb) {
        return QStringLiteral("%1 MB").arg(bytes / double(kb * kb), 0, 'f', 2);
    }
    if (bytes >= kb) {
        return QStringLiteral("%1 KB").arg(bytes / double(kb), 0, 'f', 1);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

int percentOf(quint64 done, quint64 total)
{
    if (total == 0) return 0;
    return static_cast<int>(done * 100 / total);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_core(new ClientCore(this))
{
    //UI控件设置
    ui->setupUi(this);
    ui->serverIpEdit->setText(QStringLiteral("192.168.253.130"));
    ui->portEdit->setValidator(new QIntValidator(1, 65535, this));
    ui->portEdit->setText(QStringLiteral("8888"));   // 与 ChatRoom/ServerMain.cpp 一致
    //绑定 Ctrl + 回车 = 发送
    auto *sendShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")),
                                       ui->inputEdit);
    sendShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sendShortcut, &QShortcut::activated,this,&MainWindow::onSendClicked);

    //UI -> 核心 连接
    connect(ui->connectBtn, &QPushButton::clicked,this,&MainWindow::onConnectClicked);
    connect(ui->loginBtn, &QPushButton::clicked,this,&MainWindow::onLoginClicked);
    connect(ui->sendBtn, &QPushButton::clicked,this,&MainWindow::onSendClicked);
    connect(ui->disconnectBtn, &QPushButton::clicked,this,&MainWindow::onDisconnectClicked);
    connect(ui->uploadBtn, &QPushButton::clicked,this,&MainWindow::onUploadClicked);
    connect(ui->downloadBtn, &QPushButton::clicked,this,&MainWindow::onDownloadClicked);

    //核心 -> UI 连接
    connect(m_core, &ClientCore::connected,this,&MainWindow::onConnected);
    connect(m_core, &ClientCore::disconnected,this,&MainWindow::onDisconnected);
    connect(m_core, &ClientCore::connectionLost,this,&MainWindow::onConnectionLost);
    connect(m_core, &ClientCore::loginResult,this,&MainWindow::onLoginResult);
    connect(m_core, &ClientCore::messageReceived,this,&MainWindow::onCliMessage);
    connect(m_core, &ClientCore::connectionError,this,&MainWindow::onConnectionError);
    connect(m_core, &ClientCore::protocolError,this,&MainWindow::onProtocolError);
    connect(m_core, &ClientCore::serverError,this,&MainWindow::onServerError);
    connect(m_core, &ClientCore::repeatLogin,this,&MainWindow::onRepeatLogin);

    connect(m_core, &ClientCore::fileUploadStarted,this,&MainWindow::onUploadStarted);
    connect(m_core, &ClientCore::fileUploadProgress,this,&MainWindow::onUploadProgress);
    connect(m_core, &ClientCore::fileUploadFinished,this,&MainWindow::onUploadFinished);
    connect(m_core, &ClientCore::fileDownloadStarted,this,&MainWindow::onDownloadStarted);
    connect(m_core, &ClientCore::fileDownloadProgress,this,&MainWindow::onDownloadProgress);
    connect(m_core, &ClientCore::fileDownloadFinished,this,&MainWindow::onDownloadFinished);
    connect(m_core, &ClientCore::transferError,this,&MainWindow::onTransferError);
    connect(m_core, &ClientCore::fileListReceived,this,&MainWindow::onFileListReceived);

    //初始化UI界面
    setUiLoggedOut();
    ui->pageStack->setCurrentWidget(ui->loginPage);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onConnectClicked()
{
    //连接中/已连接时按钮变成“断开连接”，再按一次直接中断
    if (m_core->isConnected() || m_core->isConnecting()) {
        m_core->disconnectServer();
        return;
    }

    //读取文字
    const QString host = ui->serverIpEdit->text().trimmed();
    bool ok = false;
    const uint port = ui->portEdit->text().trimmed().toUInt(&ok);

    if (host.isEmpty()) {
        showTip(QStringLiteral("请输入服务器 IP"));
        return;
    }
    if (!ok || port == 0 || port > 65535) {
        showTip(QStringLiteral("端口必须是 1~65535"));
        return;
    }

    //连接过程中按钮保持可点，文字改为“断开连接”
    ui->connectBtn->setEnabled(true);
    ui->connectBtn->setText(QStringLiteral("断开连接"));
    ui->loginBtn->setEnabled(false);
    ui->connStatusLabel->setText(QStringLiteral("连接中…"));
    m_core->connectServer(host, static_cast<quint16>(port));
}

void MainWindow::onLoginClicked()
{
    const QString name = ui->userNameEdit->text().trimmed();
    if (name.isEmpty()) {
        showTip(QStringLiteral("请输入用户名"));
        return;
    }

    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QStringLiteral("登录中…"));
    m_core->login(name);
}

//状态检测 + 读资源 + 刷新资源
void MainWindow::onSendClicked()
{
    if (!m_core->isLoggedIn()) {
        showTip(QStringLiteral("尚未登录"));
        return;
    }

    const QString text = ui->inputEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    // 发送对象留空 = 广播；否则私聊指定用户
    const QString target = ui->targetEdit->text().trimmed();
    if (target.isEmpty()) {
        m_core->sendChat(text);
    } else {
        m_core->sendChatTo(target, text);
    }
    ui->inputEdit->clear();

}

//刷新资源
void MainWindow::onConnected()
{
    ui->connStatusLabel->setText(QStringLiteral("已连接"));
    ui->serverIpEdit->setEnabled(false);
    ui->portEdit->setEnabled(false);
    //保持可点，文字为“断开连接”
    ui->connectBtn->setEnabled(true);
    ui->connectBtn->setText(QStringLiteral("断开连接"));
    ui->userNameEdit->setEnabled(true);
    ui->loginBtn->setEnabled(true);
    ui->userNameEdit->setFocus();
}

void MainWindow::onDisconnected()
{
    setUiLoggedOut();
    ui->pageStack->setCurrentWidget(ui->loginPage);
    showTip(QStringLiteral("已断开连接"));
}

//服务器关闭/网络异常导致的断开：弹窗明确提示
void MainWindow::onConnectionLost()
{
    setUiLoggedOut();
    ui->pageStack->setCurrentWidget(ui->loginPage);
    showTip(QStringLiteral("与服务器的连接已中断"));
    QMessageBox::warning(this, QStringLiteral("连接中断"),
                         QStringLiteral("与服务器的连接已断开，可能服务器已关闭或网络异常。"));
}

void MainWindow::onDisconnectClicked()
{
    m_core->disconnectServer();
}

//解析命令 + 刷新资源（带错误弹窗）
void MainWindow::onLoginResult(bool ok, const QString &reason)
{
    ui->loginBtn->setText(QStringLiteral("登录"));

    if (!ok) {
        ui->loginBtn->setEnabled(true);
        QMessageBox::warning(this, QStringLiteral("登录失败"),
                             reason.isEmpty()
                                 ? QStringLiteral("用户名可能已被占用")
                                 : reason);
        return;
    }

    setWindowTitle(QStringLiteral("聊天室 - %1").arg(m_core->name()));
    ui->messageView->clear();
    ui->inputEdit->clear();
    ui->targetEdit->clear();
    ui->fileList->clear();
    ui->transferLabel->setText(QStringLiteral("空闲"));
    ui->transferBar->setValue(0);
    ui->pageStack->setCurrentWidget(ui->chatPage);
    ui->inputEdit->setFocus();

    // 登录成功后拉一次服务器文件列表
    m_core->requestFileList();
}

void MainWindow::onCliMessage(const QDateTime &time, const QString &from,
                              const QString &target, const QString &msg)
{
    appendMessage(time, from, target, msg);
}

//底层Connection已经主动断开，这里只是通知更新UI
void MainWindow::onProtocolError(const QString &reason)
{

    QMessageBox::warning(this, QStringLiteral("协议错误"), reason);
}

void MainWindow::onConnectionError(const QString &reason)
{
    // 连接失败不会触发 disconnected，这里必须自己恢复按钮
    ui->connectBtn->setEnabled(true);
    ui->connectBtn->setText(QStringLiteral("连接"));
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QStringLiteral("登录"));
    ui->connStatusLabel->setText(QStringLiteral("连接失败"));
    showTip(QStringLiteral("连接错误：%1").arg(reason));
}

void MainWindow::onRepeatLogin(const QString &name)
{
    Q_UNUSED(name);
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(QStringLiteral("登录"));
    QMessageBox::warning(this, QStringLiteral("无法登录"),
                         QStringLiteral("当前已经登录。"));
}

//底层都会断开UI层的槽主要就是更新UI资源
void MainWindow::onServerError(quint16 code, const QString &reason)
{
    showTip(QStringLiteral("服务器错误(%1)：%2").arg(code).arg(reason));
}

void MainWindow::onUploadClicked()
{
    if (!m_core->isLoggedIn()) {
        showTip(QStringLiteral("尚未登录"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择要上传的文件"));
    if (path.isEmpty())
        return;

    // 失败原因由 transferError 信号显示
    m_core->sendFile(path);
}

void MainWindow::onDownloadClicked()
{
    if (!m_core->isLoggedIn()) {
        showTip(QStringLiteral("尚未登录"));
        return;
    }

    // 优先用列表里选中的文件：用 fileId 作为下载标识，同名文件也能精确区分
    QString remote;         // 发给服务器的标识（fileId 字符串或文件名）
    QString fileName;       // 保存对话框的默认文件名
    if (auto *item = ui->fileList->currentItem()) {
        remote = item->data(Qt::UserRole).toString();
        fileName = item->data(Qt::UserRole + 1).toString();
        if (fileName.isEmpty())
            fileName = item->text().trimmed();
        if (remote.isEmpty())
            remote = fileName;      // 兜底：按文件名
    }

    // 没有选中就手动输入服务器上的文件名或 fileId
    if (remote.isEmpty()) {
        bool ok = false;
        remote = QInputDialog::getText(this,
                                       QStringLiteral("下载文件"),
                                       QStringLiteral("服务器上的文件名（或 fileId）："),
                                       QLineEdit::Normal,
                                       QString(),
                                       &ok).trimmed();
        if (!ok || remote.isEmpty())
            return;
        fileName = remote;
    }

    const QString savePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存文件"),
        QDir::homePath() + QLatin1Char('/') + fileName);
    if (savePath.isEmpty())
        return;

    ui->transferLabel->setText(QStringLiteral("请求下载 %1 …").arg(fileName));
    ui->transferBar->setValue(0);
    m_core->downloadFile(remote, savePath);
}

void MainWindow::onUploadStarted(const QString &name, quint64 totalBytes)
{
    ui->transferBar->setValue(0);
    ui->transferLabel->setText(
        QStringLiteral("上传 %1 (%2)").arg(name, formatBytes(totalBytes)));
}

void MainWindow::onUploadProgress(quint64 sentBytes, quint64 totalBytes)
{
    ui->transferBar->setValue(percentOf(sentBytes, totalBytes));
    ui->transferLabel->setText(
        QStringLiteral("上传 %1%  %2 / %3")
            .arg(percentOf(sentBytes, totalBytes))
            .arg(formatBytes(sentBytes), formatBytes(totalBytes)));
}

void MainWindow::onUploadFinished(const QString &name, bool ok, const QString &reason)
{
    if (ok) {
        ui->transferBar->setValue(100);
        ui->transferLabel->setText(QStringLiteral("上传完成：%1").arg(name));
        showTip(QStringLiteral("文件已上传：%1").arg(name));
        // 上传成功后刷新侧边栏，新文件由服务器列表带回来
        m_core->requestFileList();
    } else {
        ui->transferBar->setValue(0);
        ui->transferLabel->setText(
            QStringLiteral("上传失败：%1").arg(reason.isEmpty() ? QStringLiteral("未知错误") : reason));
        showTip(QStringLiteral("上传失败：%1").arg(reason));
    }
}

void MainWindow::onDownloadStarted(const QString &name, quint64 totalBytes)
{
    ui->transferBar->setValue(0);
    ui->transferLabel->setText(
        QStringLiteral("下载 %1 (%2)").arg(name, formatBytes(totalBytes)));
}

void MainWindow::onDownloadProgress(quint64 receivedBytes, quint64 totalBytes)
{
    ui->transferBar->setValue(percentOf(receivedBytes, totalBytes));
    ui->transferLabel->setText(
        QStringLiteral("下载 %1%  %2 / %3")
            .arg(percentOf(receivedBytes, totalBytes))
            .arg(formatBytes(receivedBytes), formatBytes(totalBytes)));
}

void MainWindow::onDownloadFinished(const QString &name, bool ok, const QString &reason,
                                    const QString &savePath)
{
    if (ok) {
        ui->transferBar->setValue(100);
        ui->transferLabel->setText(QStringLiteral("下载完成：%1").arg(name));
        showTip(QStringLiteral("文件已保存：%1").arg(savePath));
    } else {
        ui->transferBar->setValue(0);
        ui->transferLabel->setText(
            QStringLiteral("下载失败：%1").arg(reason.isEmpty() ? QStringLiteral("未知错误") : reason));
        showTip(QStringLiteral("下载失败：%1").arg(reason));
    }
}

void MainWindow::onTransferError(const QString &reason)
{
    ui->transferBar->setValue(0);
    ui->transferLabel->setText(reason);
    showTip(reason);
}

void MainWindow::setUiLoggedOut()
{
    ui->serverIpEdit->setEnabled(true);
    ui->portEdit->setEnabled(true);
    ui->connectBtn->setEnabled(true);
    ui->connectBtn->setText(QStringLiteral("连接"));
    ui->userNameEdit->setEnabled(false);
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QStringLiteral("登录"));
    ui->connStatusLabel->setText(QStringLiteral("未连接"));
    ui->transferLabel->setText(QStringLiteral("空闲"));
    ui->transferBar->setValue(0);
    ui->fileList->clear();
    setWindowTitle(QStringLiteral("MainWindow"));
}

//服务器文件列表：整个侧边栏按服务器索引重建
void MainWindow::onFileListReceived(const QList<RemoteFileInfo> &files)
{
    ui->fileList->clear();
    for (const RemoteFileInfo &info : files) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  (%2)").arg(info.name, formatBytes(info.size)),
            ui->fileList);
        item->setData(Qt::UserRole, info.id);         // 下载时发给服务器的 fileId
        item->setData(Qt::UserRole + 1, info.name);   // 原始文件名（保存对话框用）
        item->setToolTip(QStringLiteral("fileId: %1").arg(info.id));
    }
}

//处理文本，转为HTML显示
void MainWindow::appendMessage(const QDateTime &time, const QString &from,
                               const QString &target, const QString &msg)
{
    // 消息内容是用户输入，先转义再拼 HTML，防止HTML注入
    QString body = msg.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

    const bool own = m_core->isLoggedIn() && from == m_core->name();
    const bool isPrivate = !target.isEmpty();

    QString sender = from.toHtmlEscaped();
    // 自己发的消息（服务器会回显给发送者）用颜色标出来
    if (own) {
        sender = QStringLiteral("<span style=\"color:#1565c0\">%1</span>").arg(sender);
    }

    // [私聊]/[广播] 放在发送者与正文的换行之前；自己的私聊标注发给了谁
    QString tag;
    if (isPrivate) {
        if (own) {
            tag = QStringLiteral("<span style=\"color:#ef6c00\">[私聊→%1]</span>")
                      .arg(target.toHtmlEscaped());
        } else {
            tag = QStringLiteral("<span style=\"color:#ef6c00\">[私聊]</span>");
        }
    } else {
        tag = QStringLiteral("<span style=\"color:#888\">[广播]</span>");
    }

    //追加显示[时间(灰)] 发送者 [私聊/广播] \n 消息
    ui->messageView->append(
        QStringLiteral("<span style=\"color:#888\">[%1]</span> <b>%2</b> %3<br>%4")
            .arg(time.toString(QStringLiteral("HH:mm:ss")),
                 sender,
                 tag,
                 body));

    // 追加后滚到最底部(QTextBrowser自带滚动条)
    auto *bar = ui->messageView->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::showTip(const QString &text)
{
    ui->statusBar->showMessage(text,5000);
}

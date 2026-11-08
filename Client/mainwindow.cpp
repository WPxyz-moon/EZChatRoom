#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "clientcore.h"

#include <QIntValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_core(new ClientCore(this))
{
    //UI控件设置
    ui->setupUi(this);
    ui->serverIpEdit->setText(QStringLiteral("192.168.253.130"));
    ui->portEdit->setValidator(new QIntValidator(1, 65535, this));
    ui->portEdit->setText(QStringLiteral("6767"));
    //绑定 Ctrl + 回车 = 发送
    auto *sendShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")),
                                       ui->inputEdit);
    sendShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sendShortcut, &QShortcut::activated,this,&MainWindow::onSendClicked);

    //UI -> 核心 连接
    connect(ui->connectBtn, &QPushButton::clicked,this,&MainWindow::onConnectClicked);
    connect(ui->loginBtn, &QPushButton::clicked,this,&MainWindow::onLoginClicked);
    connect(ui->sendBtn, &QPushButton::clicked,this,&MainWindow::onSendClicked);


    //核心 -> UI 连接
    connect(m_core, &ClientCore::connected,this,&MainWindow::onConnected);
    connect(m_core, &ClientCore::disconnected,this,&MainWindow::onDisconnected);
    connect(m_core, &ClientCore::loginResult,this,&MainWindow::onLoginResult);
    connect(m_core, &ClientCore::messageReceived,this,&MainWindow::onCliMessage);
    connect(m_core, &ClientCore::connectionError,this,&MainWindow::onConnectionError);
    connect(m_core, &ClientCore::protocolError,this,&MainWindow::onProtocolError);
    connect(m_core, &ClientCore::serverError,this,&MainWindow::onServerError);
    connect(m_core, &ClientCore::repeatLogin,this,&MainWindow::onRepeatLogin);

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

    //更新按钮和标签状态
    ui->connectBtn->setEnabled(false);
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

    m_core->sendChat(text);
    ui->inputEdit->clear();

}

//刷新资源
void MainWindow::onConnected()
{
    ui->connStatusLabel->setText(QStringLiteral("已连接"));
    ui->serverIpEdit->setEnabled(false);
    ui->portEdit->setEnabled(false);
    ui->connectBtn->setEnabled(false);
    ui->userNameEdit->setEnabled(true);
    ui->loginBtn->setEnabled(true);
    ui->userNameEdit->setFocus();
}

void MainWindow::onDisconnected()
{
    setUiLoggedOut();
    ui->pageStack->setCurrentWidget(ui->loginPage);
    showTip(QStringLiteral("与服务器断开连接"));
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
    ui->pageStack->setCurrentWidget(ui->chatPage);
    ui->inputEdit->setFocus();
}

void MainWindow::onCliMessage(const QDateTime &time, const QString &from, const QString &msg)
{
    appendMessage(time, from, msg);
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

void MainWindow::setUiLoggedOut()
{
    ui->serverIpEdit->setEnabled(true);
    ui->portEdit->setEnabled(true);
    ui->connectBtn->setEnabled(true);
    ui->userNameEdit->setEnabled(false);
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QStringLiteral("登录"));
    ui->connStatusLabel->setText(QStringLiteral("未连接"));
    setWindowTitle(QStringLiteral("MainWindow"));
}

//处理文本，转为HTML显示
void MainWindow::appendMessage(const QDateTime &time, const QString &from, const QString &msg)
{
    // 消息内容是用户输入，先转义再拼 HTML，防止HTML注入
    QString body = msg.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

    //追加显示[时间(灰)] 发送者 \n 消息
    ui->messageView->append(
        QStringLiteral("<span style=\"color:#888\">[%1]</span> <b>%2</b><br>%3")
            .arg(time.toString(QStringLiteral("HH:mm:ss")),
                 from.toHtmlEscaped(),
                 body));

    // 追加后滚到最底部(QTextBrowser自带滚动条)
    auto *bar = ui->messageView->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::showTip(const QString &text)
{
    ui->statusBar->showMessage(text,5000);
}

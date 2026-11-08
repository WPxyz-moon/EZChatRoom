#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>

class ClientCore;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    //UI -> 核心
    void onConnectClicked();
    void onLoginClicked();
    void onSendClicked();

    //核心 -> UI
    void onConnected();
    void onDisconnected();
    void onLoginResult(bool ok,const QString& reason);
    void onCliMessage(const QDateTime& time,const QString& from,const QString& msg);

    void onProtocolError(const QString& reason);
    void onConnectionError(const QString& reason);
    void onRepeatLogin(const QString& name);
    void onServerError(quint16 code,const QString& reason);

private:
    //辅助界面显示
    void setUiLoggedOut();
    void appendMessage(const QDateTime& time,const QString& from,const QString& msg);
    void showTip(const QString &text);

    Ui::MainWindow *ui;
    ClientCore* m_core = nullptr;
};
#endif // MAINWINDOW_H

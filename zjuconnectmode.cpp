#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "loginwindow/loginwindow.h"
#include "utils/utils.h"

void MainWindow::initZjuConnect()
{
    clearLog();

    zjuConnectController = new ZjuConnectController();

    disconnect(ui->pushButton1, &QPushButton::clicked, nullptr, nullptr);
    ui->pushButton1->setText("连接服务器");
    disconnect(ui->pushButton2, &QPushButton::clicked, nullptr, nullptr);
    ui->pushButton2->setText("设置系统代理");
    ui->pushButton2->hide();

    // 连接服务器
    connect(zjuConnectController, &ZjuConnectController::outputRead, this,
        [&](const QString &output)
        {
            ui->logPlainTextEdit->appendPlainText(output.trimmed());
        });

    connect(zjuConnectController, &ZjuConnectController::error, this,
        [&](ZJU_ERROR err)
        {
            if (zjuConnectError == ZJU_ERROR::NONE)
            {
                zjuConnectError = err;
            }
        });

    connect(zjuConnectController, &ZjuConnectController::finished, this, [&]()
    {
        if (
            zjuConnectError != ZJU_ERROR::NONE &&
            zjuConnectError != ZJU_ERROR::OTHER &&
            settings->value("ZJUConnect/AutoReconnect", false).toBool() &&
            isZjuConnectLinked
            )
        {
            QTimer::singleShot(settings->value("ZJUConnect/ReconnectTime", 1).toInt() * 1000, this, [&]()
            {
                if (isZjuConnectLinked)
                {
                    zjuConnectController->stop();

                    isZjuConnectLinked = false;
                    ui->pushButton1->click();
                }
            });

            return;
        }

        addLog("VPN 断开！");
        showNotification("VPN", "VPN 断开！", QSystemTrayIcon::MessageIcon::Warning);
        isZjuConnectLinked = false;
        ui->pushButton1->setText("连接服务器");
        if (isSystemProxySet)
        {
            ui->pushButton2->click();
        }
        ui->pushButton2->hide();

        switch (zjuConnectError)
        {
        case ZJU_ERROR::INVALID_DETAIL:
            QMessageBox::critical(this, "错误", "登录失败！\n请检查设置中的网络账号和密码是否设置正确。");
            break;
        case ZJU_ERROR::BRUTE_FORCE:
            QMessageBox::critical(this, "错误", "登录失败！\n登录尝试过于频繁，IP 被风控，请稍后重试或换用 EasyConnect。");
            break;
        case ZJU_ERROR::OTHER_LOGIN_FAILED:
            QMessageBox::critical(this, "错误", "登录失败！\n未知原因，可将日志反馈给开发者以便调查。");
            break;
        case ZJU_ERROR::ACCESS_DENIED:
            QMessageBox::critical(this, "错误", "权限不足！\n请关闭程序，点击右键以管理员身份运行。");
            break;
        case ZJU_ERROR::LISTEN_FAILED:
            QMessageBox::critical(this, "错误", "监听失败！\n请关闭占用端口的程序（如残留的 zju-connect.exe），或者监听其它端口。");
            break;
        case ZJU_ERROR::CLIENT_FAILED:
            QMessageBox::critical(this, "错误", "连接失败！\n可能是响应超时，请检查本地网络配置是否正常，服务器设置是否正确。");
            break;
        case ZJU_ERROR::PROGRAM_NOT_FOUND:
            QMessageBox::critical(this, "错误", "程序未找到！\n请检查核心是否在正确路径下，检查是否解压在当前目录下。");
            break;
        case ZJU_ERROR::OTHER:
            QMessageBox::critical(this, "错误", "其它错误！\n未知原因，可将日志反馈给开发者以便调查。");
            break;
        case ZJU_ERROR::NONE:
        default:
            break;
        }
        zjuConnectError = ZJU_ERROR::NONE;
    });

    connect(ui->pushButton1, &QPushButton::clicked,
            [&]()
            {
                if (!isZjuConnectLinked)
                {
                    if (settings->contains("ZJUConnect/ServerAddress") &&
                        settings->value("ZJUConnect/ServerAddress").toString().isEmpty())
                    {
                        QMessageBox::critical(this, "错误", "服务器地址不能为空");
                        return;
                    }

                    QString username_ = settings->value("Common/Username", "").toString();
                    QString password_ = QByteArray::fromBase64(settings->value("Common/Password", "").toString().toUtf8());

                    auto startZjuConnect = [this](const QString &username, const QString &password) {
                        QString program_filename;
                        if (QSysInfo::productType() == "windows")
                        {
                            program_filename = "zju-connect.exe";
                        }
                        else
                        {
                            program_filename = "zju-connect";
                        }
                        QString program_path = QCoreApplication::applicationDirPath() + "/" + program_filename;
						QString bind_prefix = settings->value("ZJUConnect/OutsideAccess", false).toBool() ? "0.0.0.0:" : "127.0.0.1:";

                        isZjuConnectLinked = true;
                        ui->pushButton1->setText("断开服务器");
                        ui->pushButton2->show();

                        if (settings->value("ZJUConnect/AutoSetProxy", false).toBool())
                        {
                            ui->pushButton2->click();
                        }

                        zjuConnectController->start(
                            program_path,
                            username,
                            password,
                            settings->value("ZJUConnect/ServerAddress", "vpn.hitsz.edu.cn").toString(),
                            settings->value("ZJUConnect/ServerPort", 443).toInt(),
                            settings->value("ZJUConnect/DNS").toString(),
                            settings->value("ZJUConnect/SecondaryDNS", "").toString(),
                            !settings->value("ZJUConnect/MultiLine", false).toBool(),
                            !settings->value("ZJUConnect/KeepAlive", false).toBool(),
                            settings->value("ZJUConnect/ProxyAll", false).toBool(),
                            bind_prefix + QString::number(settings->value("ZJUConnect/Socks5Port", 11080).toInt()),
                            bind_prefix + QString::number(settings->value("ZJUConnect/HttpPort", 11081).toInt()),
                            settings->value("ZJUConnect/ShadowsocksUrl", "").toString(),
                            settings->value("ZJUConnect/TunMode", false).toBool(),
                            settings->value("ZJUConnect/Route", false).toBool(),
                            settings->value("ZJUConnect/DNSHijack", false).toBool(),
                            settings->value("ZJUConnect/Debug", false).toBool(),
                            settings->value("ZJUConnect/TcpPortForwarding", "").toString(),
                            settings->value("ZJUConnect/UdpPortForwarding", "").toString()
                        );
                	};

                    if (username_.isEmpty() || password_.isEmpty())
                    {
                        login_window = new LoginWindow(this);
                        login_window->setDetail(username_, password_);

                        connect(login_window, &LoginWindow::login, this,
                            [&, startZjuConnect](const QString& username, const QString& password, bool saveDetail)
                            {
                                if (saveDetail)
                                {
                                    settings->setValue("Common/Username", username);
                                    settings->setValue("Common/Password", QString(password.toUtf8().toBase64()));
                                    settings->sync();
                                }
                                startZjuConnect(username, password);
                            }
                        );
                        login_window->show();
                    }
                    else
                    {
                        startZjuConnect(username_, password_);
                    }
                }
                else
                {
                    isZjuConnectLinked = false;

                    zjuConnectController->stop();

                    if (isSystemProxySet)
                    {
                        ui->pushButton2->click();
                    }

                    ui->pushButton1->setText("连接服务器");
                    ui->pushButton2->hide();
                }
            });

    // 设置系统代理
    connect(ui->pushButton2, &QPushButton::clicked,
            [&]()
            {
                if (!isSystemProxySet)
                {
                    if (Utils::isSystemProxySet())
                    {
                        int rtn = QMessageBox::warning(this, "警告",
                            "当前已存在系统代理配置（可能是 Clash 或其它代理软件）\n是否覆盖当前系统代理配置？",
                            QMessageBox::Yes | QMessageBox::No);

                        if (rtn == QMessageBox::No)
                        {
                            return;
                        }
                    }

                    Utils::setSystemProxy(settings->value("ZJUConnect/HttpPort", 11081).toInt(),
                                          settings->value("ZJUConnect/Socks5Port", 11080).toInt(),
                                          settings->value("ZJUConnect/SystemProxyBypass", "localhost").toString());
                    ui->pushButton2->setText("清除系统代理");
                    isSystemProxySet = true;
                }
                else
                {
                    Utils::clearSystemProxy();
                    ui->pushButton2->setText("设置系统代理");
                    isSystemProxySet = false;
                    if (!isZjuConnectLinked)
                    {
                        ui->pushButton2->hide();
                    }
                }
            });

    emit SetModeFinished();
}

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include "ConnectManager.h"
#include <QQmlContext>
#include "ModelManager.h"
#include "RequestManager.h"
#include "MsgManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    // 将模型实例暴露给QML
    engine.rootContext()->setContextProperty("chatitemlistmodel", CHATITEMMODEL);
    engine.rootContext()->setContextProperty("sessionmodel", SESSIONMODEL);
    engine.rootContext()->setContextProperty("userinfomodel", USERINFOMODEL);
    engine.rootContext()->setContextProperty("loginmodel", LOGINMODEL);
    engine.load(url);

    // 创建局部事件循环和超时控制器等待子线程完成初始化
    QEventLoop localLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(10000);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &localLoop,
                     &QEventLoop::quit);

    // 创建工作线程处理网络连接
    QThread* connectWorkerThread = new QThread;
    // 连接线程启动信号
    QObject::connect(connectWorkerThread, &QThread::started, [&localLoop,connectWorkerThread]() {
    // 在 workerThread 中创建对象树（确保所有对象属于该线程）
            CONNECTMANAGER;
            // 此时 worker 及其所有子对象均属于 workerThread
            // qDebug() << "Worker thread:" << CONNECTMANAGER->thread();
            QMetaObject::invokeMethod(&localLoop, "quit");
    });
    connectWorkerThread->start();

    // 进入局部事件循环（非阻塞式等待）
    localLoop.exec();

    CONNECTMANAGER->Login("192.168.58.128",8888);

    _sleep(500);

    if (CONNECTMANAGER->connectStatus() == ConnectStatus::connected)
    {
        LOGINMODEL->LoginSuccess();

        // 创建定时器
        QTimer *timer = new QTimer();

        // 连接定时器的 timeout 信号到槽函数
        QObject::connect(timer, &QTimer::timeout, [&]() {
            REQUESTMANAGER->RequestOnlineUserData();
        });
        timer->start(3000);
    }
    else {
        LOGINMODEL->LoginFail();
    }

    return app.exec();
}

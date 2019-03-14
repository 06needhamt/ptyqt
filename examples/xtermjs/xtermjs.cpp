#include <QCoreApplication>
#include <QWebSocketServer>
#include <QWebSocket>
#include "ptyqt.h"
#include <QTimer>
#include <QProcessEnvironment>

#define PORT 4242

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    //start WebSockets server for receive connections from xterm.js
    QWebSocketServer wsServer("TestServer", QWebSocketServer::NonSecureMode);
    if (!wsServer.listen(QHostAddress::Any, PORT))
        return 1;

    QMap<QWebSocket *, IPtyProcess *> sessions;

    //create new session on new connection
    QObject::connect(&wsServer, &QWebSocketServer::newConnection, [&wsServer, &sessions]()
    {
        //handle new connection
        QWebSocket *wSocket = wsServer.nextPendingConnection();

        //use cmd.exe or bash, depends on target platform
        IPtyProcess::PtyType ptyType = IPtyProcess::WinPty;
        QString shellPath = "c:\\Windows\\system32\\cmd.exe";
#ifdef Q_OS_UNIX
        shellPath = "/bin/bash";
        ptyType = IPtyProcess::UnixPty;
#endif

        //create new Pty instance
        IPtyProcess *pty = PtyQt::createPtyProcess(ptyType);

        qDebug() << "New connection" << wSocket->peerAddress() << wSocket->peerPort() << pty->pid();

        //start Pty process ()
        pty->startProcess(shellPath, QProcessEnvironment::systemEnvironment().toStringList(), 80, 24);

        //connect read channel from Pty process to write channel on websocket
        QObject::connect(pty->notifier(), &QIODevice::readyRead, [wSocket, pty]()
        {
            wSocket->sendTextMessage(pty->readAll());
        });

        //connect read channel of Websocket to write channel of Pty process
        QObject::connect(wSocket, &QWebSocket::textMessageReceived, [wSocket, pty](const QString &message)
        {
            pty->write(message.toUtf8());
        });

        //...
        //for example handle disconnections, process crashes and stuff like that...
        //...

        //add connection to list of active connections
        sessions.insert(wSocket, pty);
    });

    //stop eventloop if needed
    //QTimer::singleShot(5000, [](){ qApp->quit(); });

    //exec eventloop
    bool res = app.exec();

    QMapIterator<QWebSocket *, IPtyProcess *> it(sessions);
    while (it.hasNext())
    {
        it.next();

        it.key()->deleteLater();
        delete it.value();
    }
    sessions.clear();

    return res;
}

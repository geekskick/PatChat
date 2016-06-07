#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QMainWindow>
#include <QInputDialog>
#include <QThread>
#include <QMutexLocker>
#include <QMutex>
#include "chatserver.h"

namespace Ui {
    class ChatClient;
}

class HeartBeat : public QObject{
    Q_OBJECT

public slots:
    //void respondToHeartBeat();
};

class ChatClient : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChatClient(QWidget *parent = 0);
    ~ChatClient();

signals:
    void destinationDetailsChanged(QString ip, int port);
    void chatChangedState(bool enabled);
    void heartbeatRecd();

private slots:
    void inputServerDetails();
    void updateStatus(QAbstractSocket::SocketState);
    void connectToServer();
    void updateStatus(QAbstractSocket::SocketError);
    void sendClicked();
    void updateTextOutput();
    void enableChat(bool);

private:
    Ui::ChatClient *ui;
    HeartBeat *hb;
    ChatServer *server;
    QString serverAddress;
    int portNumber;
    QStringList validCommands;



};

#endif // CHATCLIENT_H

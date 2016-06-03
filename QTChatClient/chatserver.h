#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpSocket>
#include <QString>
#include <QHostAddress>

class ChatServer: public QTcpSocket{
    Q_OBJECT

private:
    QString _ipAddr;
    int     _port;

public:
    explicit ChatServer(QObject *parent = 0, QString ip = "127.0.0.1", int port = 51718);
    //~ChatServer();
    enum{COMMAND, ARGS1, ARGS2};
    const char DEST_MARKER = '$';

    static QStringList getValidCommands();

signals:
    void reConnect();

public slots:
    void connectToServer();
    void setIPAddressAndPort(QString ip, int port);
    void disconnectSocket();

};


#endif // CHATSERVER_H

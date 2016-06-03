#include "chatserver.h"


ChatServer::ChatServer(QObject *parent, QString ip, int port):QTcpSocket(parent){
    this->_ipAddr = ip;
    this->_port = port;

    this->connect(this, SIGNAL(reConnect()),this, SLOT(connectToServer()));
}

void ChatServer::setIPAddressAndPort(QString ip, int port){
    qDebug()<<"changing destination";

    SocketState s = this->state();
    this->_ipAddr = ip;
    this->_port = port;

    if(s == ConnectedState){
        qDebug() << "Reconnecting";
        emit disconnectFromHost();
        emit reConnect();
    }
}

void ChatServer::disconnectSocket(){
    this->disconnectFromHost();
}

void ChatServer::connectToServer(){
    qDebug() << "Connecting to: " << this->_ipAddr;

    this->connectToHost(QHostAddress(this->_ipAddr), this->_port);
    this->waitForConnected();
}

QStringList ChatServer::getValidCommands(){
    QStringList l = { "LOGIN", "NEWUSER", "PING", "SEND", "LOGOUT" };
    return l;
}

void ChatServer::handleHeartbeat(){
    this->write(QString("ACK").toUtf8());
}

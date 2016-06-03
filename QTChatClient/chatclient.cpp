#include "chatclient.h"
#include "ui_chatclient.h"

ChatClient::ChatClient(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ChatClient)
{
    ui->setupUi(this);
    ui->pushButton_reconnect->setText("Connect");

    server = new ChatServer();
    this->validCommands = ChatServer::getValidCommands();

    this->connect(server, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(updateStatus(QAbstractSocket::SocketState)));
    this->connect(server, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(updateStatus(QAbstractSocket::SocketError)));
    this->connect(this, SIGNAL(destinationDetailsChanged(QString,int)), server, SLOT(setIPAddressAndPort(QString,int)));
    this->connect(server, SIGNAL(readyRead()), this, SLOT(updateTextOutput()));
    this->connect(ui->pushButton_closeConn, SIGNAL(pressed()), server, SLOT(disconnectSocket()));
    this->connect(ui->pushButton_quit, SIGNAL(pressed()), this, SLOT(close()));
    this->connect(this, SIGNAL(chatOn(bool)), this, SLOT(enableChat(bool)));
    this->connect(this, SIGNAL(heartbeatRecd()), server, SLOT(handleHeartbeat()));

}

void ChatClient::sendClicked(){

    if(server->state() == QAbstractSocket::ConnectedState){
        qDebug() << "sending";
        server->write(ui->lineEdit_input->text().toUtf8());
    }
    ui->lineEdit_input->clear();
}

void ChatClient::updateTextOutput(){
    QString msg = server->readAll();

    if(msg == "BEAT"){
        emit heartbeatRecd();
    }
    else{
        ui->textBrowser_output->append(msg);
    }
}

ChatClient::~ChatClient()
{
    delete ui;
}

void ChatClient::inputServerDetails(){
    ui->statusBar->showMessage("input clicked", 1000);
    bool ok;

    //showdialog box
    serverAddress = QInputDialog::getText(
                        this,
                        tr("Server"),
                        tr("Enter Server"),
                        QLineEdit::Normal,
                        tr("127.0.0.1"),
                        &ok
                        );

    portNumber = QInputDialog::getInt(
                this,
                tr("Server"),
                tr("Enter Port Num"),
                51718
                );

    emit destinationDetailsChanged(serverAddress, portNumber);
}

void ChatClient::enableChat(bool chatEnabled){

    ui->pushButton_closeConn->setEnabled(chatEnabled);
    ui->textBrowser_output->setEnabled(chatEnabled);
    ui->pushButton_reconnect->setEnabled(!chatEnabled);
    ui->lineEdit_input->setEnabled(chatEnabled);
    ui->pushButton_send->setEnabled(chatEnabled);

}



void ChatClient::updateStatus(QAbstractSocket::SocketState s){

    QString msg = "";
    QString buttonText = "Connect";
    bool chatEnabled = false;

    switch(s){
        case QAbstractSocket::UnconnectedState:
            msg = tr("Not Connected");
            break;
        case QAbstractSocket::ConnectingState:
            msg = tr("Connecting");
            break;
        case QAbstractSocket::ConnectedState:
            msg = tr("Connected");
            buttonText = "Reconnect";
            chatEnabled = true;
            break;
        default:
            break;
    }

    ui->pushButton_reconnect->setText(buttonText);
    ui->statusBar->showMessage(msg, 5000);

    emit chatOn(chatEnabled);


}

void ChatClient::updateStatus(QAbstractSocket::SocketError e){
    QObject *s = sender();

    //check the sender is the server, then display error message on the status bar
    if(s == server){
        QTcpSocket *sock = qobject_cast<QTcpSocket*>(s);
        ui->statusBar->showMessage(sock->errorString(), 5000);
    }

}

void ChatClient::connectToServer(){
    server->connectToServer();
}

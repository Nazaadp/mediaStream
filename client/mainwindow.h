#pragma once
#include <QMainWindow>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void sendAddCommand();
    void sendStatusCommand();
    void onSocketReadyRead();

private:
    void connectToServer();
    
    QTcpSocket *socket;
    QLineEdit *magnetInput;
    QTextEdit *statusLog;
    QString serverIp = "192.168.1.X"; // CAMBIA ESTO A LA IP DE TU UBUNTU
    int serverPort = 12345;
};
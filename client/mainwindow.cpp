#include "mainwindow.h"
#include <QMessageBox>
#include <QHostAddress>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // 1. Configuración de UI
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Estilo "Moderno" simple con CSS de Qt
    this->setStyleSheet("background-color: #2D2D30; color: #F1F1F1; font-family: Segoe UI;");

    QLabel *title = new QLabel("MediaStream Client");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #007ACC;");
    layout->addWidget(title);

    magnetInput = new QLineEdit();
    magnetInput->setPlaceholderText("Pegar Magnet Link aquí...");
    magnetInput->setStyleSheet("padding: 5px; background-color: #3E3E42; border: 1px solid #555;");
    layout->addWidget(magnetInput);

    QPushButton *btnDownload = new QPushButton("Iniciar Descarga");
    btnDownload->setStyleSheet("background-color: #007ACC; color: white; padding: 8px; border: none;");
    layout->addWidget(btnDownload);

    QPushButton *btnStatus = new QPushButton("Actualizar Estado / Lista");
    btnStatus->setStyleSheet("background-color: #CA5100; color: white; padding: 8px; border: none;");
    layout->addWidget(btnStatus);

    statusLog = new QTextEdit();
    statusLog->setReadOnly(true);
    statusLog->setStyleSheet("background-color: #1E1E1E; border: 1px solid #333;");
    layout->addWidget(statusLog);

    // 2. Lógica de Red
    socket = new QTcpSocket(this);

    // 3. Conexiones Señal/Slot
    connect(btnDownload, &QPushButton::clicked, this, &MainWindow::sendAddCommand);
    connect(btnStatus, &QPushButton::clicked, this, &MainWindow::sendStatusCommand);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
}

MainWindow::~MainWindow() {}

void MainWindow::connectToServer() {
    if(socket->state() != QAbstractSocket::ConnectedState) {
        socket->connectToHost(serverIp, serverPort);
        if(!socket->waitForConnected(3000)) {
            statusLog->append("Error: No se pudo conectar al servidor Ubuntu.");
        }
    }
}

void MainWindow::sendAddCommand() {
    connectToServer();
    QString magnet = magnetInput->text().trimmed();
    if(magnet.isEmpty()) return;

    QString cmd = "ADD " + magnet + "\n";
    socket->write(cmd.toUtf8());
}

void MainWindow::sendStatusCommand() {
    connectToServer();
    QString cmd = "STATUS\n";
    socket->write(cmd.toUtf8());
}

void MainWindow::onSocketReadyRead() {
    QByteArray data = socket->readAll();
    statusLog->append("Servidor: " + QString::fromUtf8(data).trimmed());
}
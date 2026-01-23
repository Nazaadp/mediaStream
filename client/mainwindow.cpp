#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QHostAddress>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
// --- 1. CONFIGURACIÓN VISUAL ---
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

// Fondo oscuro "Cinemático"
    this->setStyleSheet("background-color: #141414; color: #E5E5E5; font-family: 'Segoe UI';");

    QLabel *header = new QLabel("MediaStream: Available Movies");
    header->setStyleSheet("font-size: 22px; font-weight: bold; color: #E50914; margin-bottom: 10px;");
    layout->addWidget(header);


// Galería de Películas (Icon Mode = Grilla)
    movieGallery = new QListWidget();
    movieGallery->setViewMode(QListWidget::IconMode);
    movieGallery->setIconSize(QSize(120, 180)); // Tamaño póster
    movieGallery->setResizeMode(QListWidget::Adjust);
    movieGallery->setSpacing(10);
    movieGallery->setStyleSheet("QListWidget { background-color: #000; border: none; } "
                                "QListWidget::item { padding: 10px; } "
                                "QListWidget::item:selected { background-color: #333; }");
    layout->addWidget(movieGallery);


// Log de estado (más pequeño abajo)
    QLabel *logLabel = new QLabel("Server Status / Logs:");
    layout->addWidget(logLabel);
    statusLog = new QTextEdit();
    statusLog->setMaximumHeight(100);
    statusLog->setReadOnly(true);
    statusLog->setStyleSheet("background-color: #222; border: 1px solid #444; font-family: Consolas; font-size: 11px;");
    layout->addWidget(statusLog);

    QPushButton *btnRefreshStatus = new QPushButton("Check Server Status");
    btnRefreshStatus->setStyleSheet("background-color: #333; color: white; padding: 5px;");
    layout->addWidget(btnRefreshStatus);


// --- 2. LÓGICA DE RED ---
    socket = new QTcpSocket(this);
    netManager = new QNetworkAccessManager(this);

// Conexiones
    connect(movieGallery, &QListWidget::itemClicked, this, &MainWindow::onMovieClicked);
    connect(btnRefreshStatus, &QPushButton::clicked, this, &MainWindow::sendStatusCommand);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);

// Cargar películas al iniciar
    fetchMovieList();

/*
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
*/
}

MainWindow::~MainWindow() {}


// --- PARTE A: OBTENER PELÍCULAS DE LA API ---
void MainWindow::fetchMovieList() {
    statusLog->append("Fetching movies from YTS API...");
    
    // Endpoint de YTS para listar peliculas (limitado a 15 para la demo)
    //QUrl url("https://yts.mx/api/v2/list_movies.json?limit=15&sort_by=download_count");
    QNetworkRequest request(url);
    
    // Hacemos la petición GET asíncrona
    QNetworkReply *reply = netManager->get(request);
    
    // Cuando responda, ejecutamos onMovieListReceived
    connect(reply, &QNetworkReply::finished, [this, reply](){
        this->onMovieListReceived(reply);
    });
}

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
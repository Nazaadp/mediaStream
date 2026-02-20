#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QHostAddress>
#include <QLabel>
#include <QUrl>
#include <QNetworkRequest>
#include <QMediaPlayer>
#include <QVideoWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
// --- 1. CONFIGURACIÓN VISUAL ---
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

// Fondo oscuro "Cinemático"
    this->setStyleSheet("background-color: #141414; color: #E5E5E5; font-family: 'Segoe UI';");

    QLabel *header = new QLabel("MediaStream: Movies");
    header->setStyleSheet("font-size: 22px; font-weight: bold; color: #E50914; margin-bottom: 10px;");
    layout->addWidget(header);

// Reproductor de Video (Inicialmente oculto)
    videoWidget = new QVideoWidget();
    mediaPlayer = new QMediaPlayer(this);
    mediaPlayer->setVideoOutput(videoWidget);
    videoWidget->setMinimumHeight(400);
    videoWidget->hide();
    layout->addWidget(videoWidget);

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

// Log de estado
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
    netManager = new QNetworkAccessManager(this);

// Conexiones
    connect(movieGallery, &QListWidget::itemClicked, this, &MainWindow::onMovieClicked);
    connect(btnRefreshStatus, &QPushButton::clicked, this, &MainWindow::sendStatusCommand);

// Cargar películas al iniciar
    fetchMovieList();
}

MainWindow::~MainWindow() {}

void MainWindow::fetchMovieList() {
    statusLog->append("Fetching movies from Backend API...");
    
    // Hit our backend Discovery API (which integrates TMDB metadata)
    QUrl url(QString("http://%1:%2/api/v1/discover/movies").arg(serverIp).arg(serverPort));
    QNetworkRequest request(url);
    QNetworkReply *reply = netManager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply](){
        this->onMovieListReceived(reply);
    });
}

void MainWindow::onMovieListReceived(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        statusLog->append("Error: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray movies = doc.array();

    movieGallery->clear();

    for (const QJsonValue &value : movies) {
        QJsonObject movie = value.toObject();
        QString title = movie["title"].toString();
        // Get first hash
        QString hash;
        QJsonArray torrents = movie["torrents"].toArray();
        if(!torrents.isEmpty()) {
            hash = torrents[0].toObject()["hash"].toString();
        }
        
        QListWidgetItem *item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, hash);
        movieGallery->addItem(item);
    }
    statusLog->append("Movies and TMDB metadata loaded via Backend successfully.");
    reply->deleteLater();
}

void MainWindow::onMovieClicked(QListWidgetItem *item) {
    QString title = item->text();
    QString hash = item->data(Qt::UserRole).toString();

    statusLog->append("Selected: " + title);
    
    // Construimos magnet URI
    QString magnet = "magnet:?xt=urn:btih:" + hash + "&dn=" + QUrl::toPercentEncoding(title);

    // 1. Send Add Command to our backend
    sendAddCommand(magnet);

    // 2. Start Streaming immediately using the infoHash
    streamVideo(hash);
}

void MainWindow::sendAddCommand(const QString& magnet) {
    QUrl url(QString("http://%1:%2/api/v1/torrents").arg(serverIp).arg(serverPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["magnet_link"] = magnet;
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = netManager->post(request, data);
    
    connect(reply, &QNetworkReply::finished, [this, reply](){
        if(reply->error() == QNetworkReply::NoError) {
            statusLog->append("Backend: Torrent Added Successfully!");
        } else {
            statusLog->append("Backend Error: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::sendStatusCommand() {
    QUrl url(QString("http://%1:%2/api/v1/status").arg(serverIp).arg(serverPort));
    QNetworkRequest request(url);

    QNetworkReply *reply = netManager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply](){
        if(reply->error() == QNetworkReply::NoError) {
            statusLog->append("Backend Status: " + reply->readAll());
        } else {
            statusLog->append("Backend Error: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::streamVideo(const QString& infoHash) {
    movieGallery->hide();
    videoWidget->show();
    
    QUrl streamUrl(QString("http://%1:%2/api/v1/stream/%3").arg(serverIp).arg(serverPort).arg(infoHash));
    statusLog->append("Buffering stream: " + streamUrl.toString());
    
    mediaPlayer->setMedia(streamUrl);
    mediaPlayer->play();
}
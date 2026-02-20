#pragma once
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QVideoWidget>


struct MovieData {
    QString title;
    QString hash; // El hash es lo vital para crear el magnet
    QString coverUrl;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
// API Commands
    void sendAddCommand(const QString& magnet);
    void sendStatusCommand();

// API y UI
    void fetchMovieList();
    void onMovieListReceived(QNetworkReply *reply);
    void onMovieClicked(QListWidgetItem *item);
    
// Video Streaming
    void streamVideo(const QString& infoHash);

private:
// UI Elements
    QTextEdit *statusLog;
    QLineEdit *magnetInput;
    QListWidget *movieGallery; // Nuestra grilla de películas
    QVideoWidget *videoWidget;
    QMediaPlayer *mediaPlayer;

// Network Manager para la API
    QNetworkAccessManager *netManager;

// Configuración
    QString serverIp = "192.168.1.37";
    int serverPort = 8000;
};
#pragma once
#include <QMainWindow>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>


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
// Red (TCP Socket con Server)
    void sendAddCommand();
    void sendStatusCommand();
    void onSocketReadyRead();

// API y UI (Http con YTS)
    void fetchMovieList();
    void onMovieListReceived(QNetworkReply *reply);
    void onMovieClicked(QListWidgetItem *item);

private:
    void connectToServer();

// UI Elements
    QTcpSocket *socket;
    QTextEdit *statusLog;
    QLineEdit *magnetInput;
    QListWidget *movieGallery; // Nuestra grilla de películas

// Network Manager para la API
    QNetworkAccessManager *netManager;

// Configuración
    QString serverIp = "192.168.1.37";
    int serverPort = 8080;
};
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// Clase simple para gestionar el estado de MediaStream
class MediaServer {
public:
    MediaServer() {
        // Configuración de libtorrent
        lt::settings_pack p;
        p.set_int(lt::settings_pack::alert_mask, lt::alert_category::status | lt::alert_category::error);
        session_.apply_settings(p);
        
        // Iniciar hilo para procesar alertas de torrent (descargas finalizadas, etc.)
        torrent_thread_ = std::thread(&MediaServer::monitor_torrents, this);
    }

    ~MediaServer() {
        running_ = false;
        if (torrent_thread_.joinable()) torrent_thread_.join();
    }

    // Método para añadir un Magnet Link
    std::string add_magnet(const std::string& uri) {
        try {
            lt::add_torrent_params p = lt::parse_magnet_uri(uri);
            p.save_path = "./downloads"; // Carpeta donde se guardan los archivos
            session_.async_add_torrent(p);
            return "OK: Torrent added";
        } catch (const std::exception& e) {
            return "ERROR: " + std::string(e.what());
        }
    }

    // Obtener lista de archivos descargados (simplificado para MVP)
    std::string get_status() {

        std::string status_list = "";
        std::vector<lt::torrent_handle> handles = session_.get_torrents();

        if (handles.empty()) {
            return "No active torrents.";
        }

        for (const auto& h : handles) {
            if (!h.is_valid()) continue;

            lt::torrent_status s = h.status();

            int progress_percent = static_cast<int>(s.progress * 100);

            std::string state_str;
            if (s.is_seeding || s.is_finished) {
                state_str = "Ready (100%)";
            } else {
                state_str = "Downloading " + std::to_string(progress_percent) + "%";

                int download_rate = s.download_rate / 1000; // KB/s
                state_str += " - " + std::to_string(download_rate) + " KB/s";
            }

            status_list += s.name + " [" + state_str + "]\n";
        }

        return status_list;
    }

private:
    lt::session session_;
    std::thread torrent_thread_;
    bool running_ = true;

    void monitor_torrents() {
        while (running_) {
            std::vector<lt::alert*> alerts;
            session_.pop_alerts(&alerts);
            for (lt::alert* a : alerts) {
                // Aquí podrías loguear eventos: std::cout << a->message() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
};

// Función para manejar la conexión con el cliente
void handle_client(tcp::socket socket, MediaServer& server) {
    try {
        boost::asio::streambuf buffer;
        // Leer hasta encontrar un salto de línea
        boost::asio::read_until(socket, buffer, "\n");
        std::string command = boost::asio::buffer_cast<const char*>(buffer.data());
        
        // Limpiar el string
        command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());
        command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());

        std::cout << "Comando recibido: " << command << std::endl;

        std::string response;
        if (command == "STATUS") {
            response = server.get_status();
        } else if (command.rfind("ADD ", 0) == 0) {
            std::string magnet = command.substr(4);
            response = server.add_magnet(magnet);
        } else {
            response = "UNKNOWN COMMAND";
        }

        boost::asio::write(socket, boost::asio::buffer(response + "\n"));
    } catch (std::exception& e) {
        std::cerr << "Excepción en conexión: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        MediaServer server;

        std::cout << "MediaStream Server running on port 8080..." << std::endl;

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            // En un sistema real, usaríamos hilos o asincronía aquí.
            // Para el MVP, manejamos la petición de forma bloqueante rápida.
            handle_client(std::move(socket), server);
        }
    } catch (std::exception& e) {
        std::cerr << "Error fatal: " << e.what() << std::endl;
    }
    return 0;
}
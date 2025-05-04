#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <filesystem>
#include <system_error>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace fs   = std::filesystem;
namespace Config {
    constexpr int Port        = 8989;
    constexpr int BufferSize  = 4096;
    constexpr int Backlog     = 1;
}
using SA_IN = sockaddr_in;
using SA    = sockaddr;

struct SocketCloser {
    void operator()(int* fd) const noexcept {
        if (fd && *fd >= 0) ::close(*fd);
        delete fd;
    }
};

static std::unique_ptr<int,SocketCloser> make_socket_ptr(int raw_fd) {
    return std::unique_ptr<int,SocketCloser>(new int(raw_fd));
}

inline int check_sys(int result, const char* msg) {
    if (result < 0) {
        throw std::system_error(errno, std::generic_category(), msg);
    }
    return result;
}

class FileServer {
public:
    void run() {
        std::cout << "[+] Starting file server on port " << Config::Port << "...\n";

        auto server_sock = make_socket_ptr(
            check_sys(::socket(AF_INET, SOCK_STREAM, 0), "socket() failed")
        );

        SA_IN addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(Config::Port);

        check_sys(::bind(*server_sock, reinterpret_cast<SA*>(&addr), sizeof(addr)),
                  "bind() failed");
        check_sys(::listen(*server_sock, Config::Backlog),
                  "listen() failed");

        while (true) {
            SA_IN client_addr{};
            socklen_t addrlen = sizeof(client_addr);
            std::cout << "[+] Waiting for connection...\n";

            int client_fd = check_sys(
                ::accept(*server_sock, reinterpret_cast<SA*>(&client_addr), &addrlen),
                "accept() failed"
            );
            auto client_sock = make_socket_ptr(client_fd);

            std::cout << "[+] Connection from "
                      << inet_ntoa(client_addr.sin_addr) << ":"
                      << ntohs(client_addr.sin_port) << "\n";

            handle_connection(*client_sock);
        }
    }

private:
    void handle_connection(int client_fd) {
        // allocate buffer on the heap with unique_ptr
        auto buf = std::make_unique<char[]>(Config::BufferSize);
        ssize_t bytes_read = 0;
        int total = 0;

        std::cout << "[*] Reading request...\n";
        while ((bytes_read = ::read(client_fd, buf.get() + total,
                                    Config::BufferSize - total - 1)) > 0)
        {
            total += bytes_read;
            if (total >= Config::BufferSize - 1 || buf[total-1] == '\n')
                break;
        }
        check_sys(static_cast<int>(bytes_read), "read() failed");

        buf[total] = '\0';
        if (total > 0 && buf[total-1] == '\n') buf[total-1] = '\0';

        fs::path req_path{buf.get()};
        std::cout << "[+] Requested: " << req_path << "\n";

        try {
            fs::path canon = fs::canonical(req_path);
            std::cout << "[+] Resolved: " << canon << "\n";

            std::ifstream file(canon, std::ios::binary);
            if (!file)
                throw std::runtime_error("Failed to open file: " + canon.string());

            std::cout << "[*] Sending...\n";
            while (file) {
                file.read(buf.get(), Config::BufferSize);
                std::streamsize cnt = file.gcount();
                if (cnt > 0) {
                    std::cout << "[+] " << cnt << " bytes\n";
                    ::write(client_fd, buf.get(), static_cast<size_t>(cnt));
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "[!] FS error: " << e.what() << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "[!] Error: " << e.what() << "\n";
        }

        std::cout << "[+] Done, closing connection.\n";
    }
};

int main() {
    try {
        auto server = std::make_unique<FileServer>();
        server->run();
    }
    catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

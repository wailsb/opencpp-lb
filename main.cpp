#include <iostream>
#include <string>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
#endif

const std::string PORT = "8080";
const int BACKLOG = 10;

int main() {
    std::cout << "[INFO] Initializing Load Balancer with getaddrinfo()..." << std::endl;

    // 1. Initialize Winsock on Windows
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[ERROR] WSAStartup failed." << std::endl;
            return 1;
        }
    #endif

    // 2. Setup hints structure for getaddrinfo
    addrinfo hints{};
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4 (use AF_UNSPEC for dual-stack)
    hints.ai_socktype = SOCK_STREAM; // TCP stream
    hints.ai_protocol = IPPROTO_TCP; // TCP protocol
    hints.ai_flags = AI_PASSIVE;     // Server mode (bind to wildcard IP)

    addrinfo* result = nullptr;

    // 3. Resolve the local address and port
        int res = getaddrinfo(nullptr, PORT.c_str(), &hints, &result);    if (res != 0) {
        std::cerr << "[ERROR] getaddrinfo failed: " << res << std::endl;
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    // 4. Create the server socket using resolved data
    SOCKET serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create socket." << std::endl;
        freeaddrinfo(result);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // 5. Bind the socket using the resolved address structure
    if (bind(serverSocket, result->ai_addr, (int)result->ai_addrlen) < 0) {
        std::cerr << "[ERROR] Bind failed on port " << PORT << std::endl;
        freeaddrinfo(result);
        CLOSE_SOCKET(serverSocket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    // We no longer need the address info list once bound
    freeaddrinfo(result);

    // 6. Listen for incoming connections
    if (listen(serverSocket, BACKLOG) < 0) {
        std::cerr << "[ERROR] Listen failed." << std::endl;
        CLOSE_SOCKET(serverSocket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    std::cout << "[SUCCESS] Server is listening on port " << PORT << " using Microsoft's recommended pattern!" << std::endl;

    // Cleanup for now
    CLOSE_SOCKET(serverSocket);
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
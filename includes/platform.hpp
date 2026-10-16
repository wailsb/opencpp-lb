#pragma once

// Cross-platform socket definitions shared by all translation units

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET close
#endif

// Initializes the socket subsystem (no-op outside Windows)
inline bool initSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

// Releases the socket subsystem (no-op outside Windows)
inline void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

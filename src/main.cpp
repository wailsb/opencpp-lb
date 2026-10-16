#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include "platform.hpp"
#include "backend.hpp"
#include "task_queue.hpp"

const std::string PORT = "8080";
const int BACKLOG = 10;

// Reads (and discards) the client's request so closing the socket doesn't reset the connection
static void drainRequest(SOCKET clientSocket) {
    char buffer[4096];
    recv(clientSocket, buffer, sizeof(buffer), 0);
}

// Sends a full response, retrying on partial writes
static void sendAll(SOCKET clientSocket, const char* data) {
    size_t remaining = std::strlen(data);
    while (remaining > 0) {
        int sent = send(clientSocket, data, static_cast<int>(remaining), 0);
        if (sent <= 0) return;
        data += sent;
        remaining -= static_cast<size_t>(sent);
    }
}

// Worker function executed by background threads to process queued clients
void workerThreadFunction(TaskQueue& taskQueue, BackendPool& pool) {
    ClientTask task;
    while (taskQueue.pop(task)) { // Pulls next client from queue safely
        drainRequest(task.clientSocket);

        // 1. Check if backend pool is empty
        if (pool.isEmpty()) {
            std::cerr << "[ERROR] No handlers are up or service is down! Dropping request." << std::endl;
            sendAll(task.clientSocket, "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\nNo backend handlers available.\n");
        } else {
            // 2. Query saturation and select the best backend server
            BackendServer* target = pool.getBestBackend();
            if (target == nullptr) {
                std::cerr << "[ERROR] All backends are unhealthy! Dropping request." << std::endl;
                sendAll(task.clientSocket, "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\nAll backends down.\n");
            } else {
                std::cout << "[ROUTING] Queued client assigned to backend -> "
                          << target->host << ":" << target->port
                          << " (Saturation Score: " << target->current_load << ")" << std::endl;
                // TODO: Next step - Connect to `target` socket and proxy the data!
            }
        }

        CLOSE_SOCKET(task.clientSocket);
    }
}

int main() {
    std::cout << "[INFO] Initializing Load Balancer with Queue & Thread Pool..." << std::endl;

    if (!initSockets()) {
        std::cerr << "[ERROR] Socket subsystem initialization failed." << std::endl;
        return 1;
    }

    // Setup backend pool (registering a mock backend so we can test routing)
    BackendPool pool;
    pool.addBackend("127.0.0.1", 9001); // Comment out to test the 503 path

    // Setup shared Task Queue
    TaskQueue taskQueue;

    // Spawn a small worker thread pool (e.g., 4 threads)
    const int THREAD_POOL_SIZE = 4;
    std::vector<std::thread> workers;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        workers.emplace_back(workerThreadFunction, std::ref(taskQueue), std::ref(pool));
    }

    // Stops workers and releases resources on any exit path
    auto shutdown = [&](int code) {
        taskQueue.close();
        for (auto& worker : workers) worker.join();
        cleanupSockets();
        return code;
    };

    // Setup network socket using getaddrinfo
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* result = nullptr;
    if (getaddrinfo(nullptr, PORT.c_str(), &hints, &result) != 0) {
        std::cerr << "[ERROR] getaddrinfo failed." << std::endl;
        return shutdown(1);
    }

    SOCKET serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create socket." << std::endl;
        freeaddrinfo(result);
        return shutdown(1);
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(serverSocket, result->ai_addr, (int)result->ai_addrlen) < 0) {
        std::cerr << "[ERROR] Bind failed on port " << PORT << std::endl;
        freeaddrinfo(result);
        CLOSE_SOCKET(serverSocket);
        return shutdown(1);
    }
    freeaddrinfo(result);

    if (listen(serverSocket, BACKLOG) < 0) {
        std::cerr << "[ERROR] Listen failed." << std::endl;
        CLOSE_SOCKET(serverSocket);
        return shutdown(1);
    }

    std::cout << "[SUCCESS] Load Balancer listening on port " << PORT << ". Queue and workers active!" << std::endl;

    // Main Accept Loop
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket != INVALID_SOCKET) {
            std::cout << "[INFO] Client connection accepted. Pushing to task queue..." << std::endl;
            // Push client socket into thread-safe queue instead of blocking the accept loop
            taskQueue.push(clientSocket);
        }
    }

    // Cleanup (though loop runs indefinitely unless killed)
    CLOSE_SOCKET(serverSocket);
    return shutdown(0);
}

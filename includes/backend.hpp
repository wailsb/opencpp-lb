#pragma once

#include <string>
#include <vector>
#include <iostream>

struct BackendServer {
    std::string host;
    int port;
    bool is_healthy;
    float current_load; // Saturation score from 0.0 to 1.0

    // Simulates querying the backend's current load/saturation metric
    float querySaturation();
};

class BackendPool {
private:
    std::vector<BackendServer> backends;

public:
    void addBackend(const std::string& host, int port);
    bool isEmpty() const;
    BackendServer* getBestBackend();
};
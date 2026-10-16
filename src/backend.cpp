#include "backend.hpp"

// Returns the saturation score of the backend
float BackendServer::querySaturation() {
    if (!is_healthy) return 1.0f; // If down, max out saturation so it's never chosen
    return current_load;
}

// Adds a new backend server to the pool
void BackendPool::addBackend(const std::string& host, int port) {
    backends.push_back({host, port, true, 0.2f}); // Default initial mock load of 0.2
}

// Checks if the backend pool has any registered servers
bool BackendPool::isEmpty() const {
    return backends.empty();
}

// Iterates through all backends and returns the pointer to the least saturated healthy server
BackendServer* BackendPool::getBestBackend() {
    BackendServer* best = nullptr;
    float lowestScore = 2.0f; // Initialized higher than max possible score (1.0)

    for (auto& backend : backends) {
        if (!backend.is_healthy) continue;

        float score = backend.querySaturation();
        if (score < lowestScore) {
            lowestScore = score;
            best = &backend;
        }
    }
    return best;
}
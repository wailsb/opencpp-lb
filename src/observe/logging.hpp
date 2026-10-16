#pragma once

#include <string>

namespace lb {

// Configures the global logger; level is one of trace|debug|info|warn|error|critical|off
void init_logging(const std::string& level);

}  // namespace lb

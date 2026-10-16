#include "observe/logging.hpp"

#include <spdlog/spdlog.h>

namespace lb {

void init_logging(const std::string& level) {
    spdlog::set_level(spdlog::level::from_str(level));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [t%t] %v");
}

}  // namespace lb

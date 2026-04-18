#pragma once

#include <string>

namespace ai {

struct LlmConfig {
    std::string base_url;
    std::string api_key;
    std::string model;
    int max_tokens = 4096;
    float temperature = 0.2f;
};

}  // namespace ai

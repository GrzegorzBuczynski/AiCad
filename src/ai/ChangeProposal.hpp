#pragma once

#include <string>
#include <vector>

namespace ai {

struct ChangeProposal {
    std::string title;
    std::string description;
    std::string branch_name;
    std::string commit_message;
    std::vector<std::string> changed_files;
};

}  // namespace ai

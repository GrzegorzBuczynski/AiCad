#pragma once

#include "ai/ChangeProposal.hpp"

namespace gitops {

class LocalRepoManager {
public:
    bool create_proposal_branch(const ai::ChangeProposal& proposal);
    bool commit_proposal(const ai::ChangeProposal& proposal);
    bool run_checker();
    bool merge_proposal(const ai::ChangeProposal& proposal);
};

}  // namespace gitops

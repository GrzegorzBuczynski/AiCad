#include "git/LocalRepoManager.hpp"

namespace gitops {

bool LocalRepoManager::create_proposal_branch(const ai::ChangeProposal&) {
    return true;
}

bool LocalRepoManager::commit_proposal(const ai::ChangeProposal&) {
    return true;
}

bool LocalRepoManager::run_checker() {
    return true;
}

bool LocalRepoManager::merge_proposal(const ai::ChangeProposal&) {
    return true;
}

}  // namespace gitops

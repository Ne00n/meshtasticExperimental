#include "NextHopSelector.h"
#include "configuration.h"
#include <cstdint>

#ifndef ARDUINO
#include <chrono>
uint32_t millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}
#endif

uint8_t NextHopSelector::getBestNextHop(NodeNum dest, uint8_t currentRelay) {
    auto it = nextHopMap.find(dest);
    if (it == nextHopMap.end() || it->second.empty()) {
        LOG_DEBUG("[NextHopSelector] No candidates for dest %x", dest);
        return NO_NEXT_HOP_PREFERENCE;
    }

    // Sort the next hops by success rate and recency
    sortAndTrimNextHops(dest);
    
    // Find the best next hop that isn't the current relay
    for (const auto& candidate : it->second) {
        if (candidate.nodeId != currentRelay) {
            LOG_DEBUG("[NextHopSelector] Considering candidate for dest %x: nodeId=%x, success=%u, fail=%u, rate=%.2f", 
                     dest, candidate.nodeId, candidate.successCount, candidate.failureCount, candidate.successRate);
            
            // If we have any successes, use this candidate
            if (candidate.successCount > 0) {
                LOG_DEBUG("[NextHopSelector] Selected candidate with successes for dest %x: nodeId=%x", dest, candidate.nodeId);
                return candidate.nodeId;
            }
        }
    }

    LOG_DEBUG("[NextHopSelector] No suitable candidate found for dest %x", dest);
    return NO_NEXT_HOP_PREFERENCE;
}

void NextHopSelector::recordSuccess(NodeNum dest, uint8_t nextHop) {
    auto& candidates = nextHopMap[dest];
    
    // Find or create the candidate
    auto it = std::find_if(candidates.begin(), candidates.end(),
                          [nextHop](const NextHopCandidate& c) { return c.nodeId == nextHop; });
    
    if (it == candidates.end()) {
        LOG_DEBUG("[NextHopSelector] Adding new candidate for dest %x: nodeId=%x", dest, nextHop);
        candidates.emplace_back(nextHop);
        it = candidates.end() - 1;
    }

    // Update the candidate
    it->successCount++;
    it->lastUsedTime = millis();
    it->updateSuccessRate();
    LOG_DEBUG("[NextHopSelector] Recorded success for dest %x via nodeId=%x: success=%u, fail=%u, rate=%.2f", 
              dest, nextHop, it->successCount, it->failureCount, it->successRate);

    // Sort and trim the list
    sortAndTrimNextHops(dest);
}

void NextHopSelector::recordFailure(NodeNum dest, uint8_t nextHop) {
    auto& candidates = nextHopMap[dest];
    
    // Find or create the candidate
    auto it = std::find_if(candidates.begin(), candidates.end(),
                          [nextHop](const NextHopCandidate& c) { return c.nodeId == nextHop; });
    
    if (it == candidates.end()) {
        LOG_DEBUG("[NextHopSelector] Adding new candidate for dest %x: nodeId=%x", dest, nextHop);
        candidates.emplace_back(nextHop);
        it = candidates.end() - 1;
    }

    // Update the candidate
    it->failureCount++;
    it->lastUsedTime = millis();
    it->updateSuccessRate();
    LOG_DEBUG("[NextHopSelector] Recorded failure for dest %x via nodeId=%x: success=%u, fail=%u, rate=%.2f", 
              dest, nextHop, it->successCount, it->failureCount, it->successRate);

    // Sort and trim the list
    sortAndTrimNextHops(dest);
}

void NextHopSelector::resetNextHops(NodeNum dest) {
    nextHopMap.erase(dest);
}

void NextHopSelector::addOrUpdateNextHop(NodeNum dest, uint8_t nextHop) {
    auto& candidates = nextHopMap[dest];
    
    // Find or create the candidate
    auto it = std::find_if(candidates.begin(), candidates.end(),
                          [nextHop](const NextHopCandidate& c) { return c.nodeId == nextHop; });
    
    if (it == candidates.end()) {
        candidates.emplace_back(nextHop);
    }
}

void NextHopSelector::sortAndTrimNextHops(NodeNum dest) {
    auto& candidates = nextHopMap[dest];
    
    // Sort by success rate (primary) and recency (secondary)
    std::sort(candidates.begin(), candidates.end(),
              [](const NextHopCandidate& a, const NextHopCandidate& b) {
                  if (a.successRate != b.successRate) {
                      return a.successRate > b.successRate;
                  }
                  return a.lastUsedTime > b.lastUsedTime;
              });

    // Trim to MAX_NEXT_HOPS
    if (candidates.size() > MAX_NEXT_HOPS) {
        candidates.resize(MAX_NEXT_HOPS);
    }
} 
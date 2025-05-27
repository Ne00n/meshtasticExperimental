#include "NextHopOptimizer.h"
#include "configuration.h"
#include <algorithm>
#include <cstdint>

NextHopOptimizer::NextHopOptimizer(NodeDB *nodeDB) : nodeDB(nodeDB) {}

void NextHopOptimizer::updateRouteStats(NodeNum destination, uint8_t nextHop, uint8_t hopCount) {
    uint32_t key = getRouteKey(destination, nextHop);
    RouteStats &stats = routeStats[key];
    
    stats.totalPackets++;
    
    // Update hop statistics
    if (stats.minHops == 0 || hopCount < stats.minHops) {
        stats.minHops = hopCount;
    }
    if (hopCount > stats.maxHops) {
        stats.maxHops = hopCount;
    }
    
    // Update average hops
    stats.avgHops = ((stats.avgHops * (stats.totalPackets - 1)) + hopCount) / stats.totalPackets;
    
    // Update top routes after updating stats
    updateTopRoutes(destination);
}

void NextHopOptimizer::updateRouteSuccess(NodeNum destination, uint8_t nextHop, uint8_t hopCount) {
    uint32_t key = getRouteKey(destination, nextHop);
    RouteStats &stats = routeStats[key];
    
    // Add to recent results and maintain max size
    stats.recentResults.push_back(true);
    if (stats.recentResults.size() > MAX_PACKET_HISTORY) {
        stats.recentResults.pop_front();
    }
    
    // Update top routes after updating success
    updateTopRoutes(destination);
}

void NextHopOptimizer::updateTopRoutes(NodeNum destination) {
    std::vector<ScoredRoute> routes;
    
    // Get all known nodes that could be next hops
    for (const auto &node : nodeDB->getNodes()) {
        uint8_t potentialNextHop = nodeDB->getLastByteOfNodeNum(node.node_num);
        
        // Skip if it's the destination
        if (potentialNextHop == nodeDB->getLastByteOfNodeNum(destination)) {
            continue;
        }
        
        uint32_t key = getRouteKey(destination, potentialNextHop);
        auto it = routeStats.find(key);
        
        if (it != routeStats.end()) {
            float score = calculateRouteScore(it->second);
            if (score > 0.0f) { // Only include routes with positive scores
                routes.emplace_back(potentialNextHop, score);
            }
        }
    }
    
    // Sort routes by score (highest first)
    std::sort(routes.begin(), routes.end());
    
    // Keep only top 3 routes
    if (routes.size() > MAX_TOP_ROUTES) {
        routes.resize(MAX_TOP_ROUTES);
    }
    
    // Update the top routes map
    topRoutes[destination] = std::move(routes);
}

float NextHopOptimizer::calculateSuccessRate(const RouteStats &stats) {
    if (stats.totalPackets == 0) {
        return 0.0f;
    }
    
    // Calculate recent success rate (last MAX_PACKET_HISTORY packets)
    float recentSuccessRate = 0.0f;
    if (!stats.recentResults.empty()) {
        int successCount = std::count(stats.recentResults.begin(), stats.recentResults.end(), true);
        recentSuccessRate = static_cast<float>(successCount) / stats.recentResults.size();
    }
    
    // Since all packets we track are successful, overall success rate is 1.0
    float overallSuccessRate = 1.0f;
    
    // Weight recent success rate more heavily (70% recent, 30% overall)
    return (0.7f * recentSuccessRate) + (0.3f * overallSuccessRate);
}

float NextHopOptimizer::calculateRouteScore(const RouteStats &stats) {
    float successRate = calculateSuccessRate(stats);
    
    // Penalize routes with higher hop counts
    float hopPenalty = 1.0f - (stats.avgHops / config.lora.hop_limit);
    
    // Combine success rate and hop penalty (80% success rate, 20% hop penalty)
    return (0.8f * successRate) + (0.2f * hopPenalty);
}

uint8_t NextHopOptimizer::getBestNextHop(NodeNum destination, uint8_t currentNextHop) {
    auto it = topRoutes.find(destination);
    if (it != topRoutes.end() && !it->second.empty()) {
        // Log all top routes for debugging
        LOG_DEBUG("Top routes for 0x%x:", destination);
        for (const auto &route : it->second) {
            LOG_DEBUG("  Next hop: 0x%x, Score: %.2f", route.nextHop, route.score);
        }
        
        // Return the best route that isn't the current one
        for (const auto &route : it->second) {
            if (route.nextHop != currentNextHop) {
                return route.nextHop;
            }
        }
    }
    
    // If no better route found, keep the current one
    return currentNextHop;
}

void NextHopOptimizer::resetRouteStats(NodeNum destination, uint8_t nextHop) {
    uint32_t key = getRouteKey(destination, nextHop);
    routeStats.erase(key);
    updateTopRoutes(destination);
}

uint32_t NextHopOptimizer::getRouteKey(NodeNum destination, uint8_t nextHop) {
    return (destination << 8) | nextHop;
} 
} 
} 
#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;

    for(size_t g = 0; g < provider.GatherersCount(); ++g) {
        Gatherer gatherer = provider.GetGatherer(g);
        if(gatherer.start_pos == gatherer.end_pos) 
            continue;
        
        for(size_t i = 0; i < provider.ItemsCount(); ++i) {
            Item item = provider.GetItem(i);
            
            if(CollectionResult result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
                    result.IsCollected(gatherer.width + item.width)) {
                events.push_back({i, g, result.sq_distance, result.proj_ratio});
            }
        }
    }

    double EPSILON = 1e-5;
    std::sort(events.begin(), events.end(), [&EPSILON](const GatheringEvent& left, const GatheringEvent& right) {
        return left.time < right.time - EPSILON;
    });

    return events;
}

}  // namespace collision_detector
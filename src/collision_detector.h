#pragma once

#include "geom.h"
#include "model.h"
#include "player.h"

#include <algorithm>
#include <vector>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    double sq_distance;
    double proj_ratio;
};

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    Item(geom::Point2D position, double width) 
        : position{position}
        , width(width)
        {
        }

    geom::Point2D position;
    double width;
};

struct Gatherer {
    Gatherer(geom::Point2D start, geom::Point2D end, double width) 
        : start_pos(start)
        , end_pos(end) 
        , width(width) 
        {
        }

    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

class LootGathererProvider : public ItemGathererProvider {
public:
    LootGathererProvider(model::Game& game, players::Players& players) 
        : game_(game)
        , players_(players)
        {
        }

    size_t ItemsCount() const override {
        return game_.GetLootObjects().size();
    }
    Item GetItem(size_t idx) const override {
        
        auto& position = game_.GetLootObjects()[idx]->GetPosition();
        double width = 0.0;
        return Item({position.x, position.y}, width);
    }
    size_t GatherersCount() const override {
        return players_.GetPlayers().size();
    }
    Gatherer GetGatherer(size_t idx) const override {
        const auto& end = players_.GetPlayers().at(idx)->GetDog().GetPosition();
        const auto& start = players_.GetPlayers().at(idx)->GetDog().GetStartPosition();
        double width = 0.6;
        return Gatherer({start.x, start.y}, {end.x, end.y}, width);
    }

private:
    model::Game& game_;
    players::Players& players_;
};

class OfficeGathererProvider : public ItemGathererProvider {
public:
    OfficeGathererProvider(model::Game game, players::Players& players) 
        : game_(game)
        , players_(players)
        {
        }

    size_t ItemsCount() const override {
        size_t items_counter = 0;
        for(auto& map : game_.GetMaps()) {
            items_counter += map.GetOffices().size();
        }
        return items_counter;
    }
    Item GetItem(size_t idx) const override {
        for(auto& map : game_.GetMaps()) {
            if(idx < map.GetOffices().size()) {
                const auto& position = map.GetOffices()[idx].GetPosition();
                double offices_width = 0.5;
                return Item{{position.x * 1.0, position.y * 1.0}, offices_width};
            }
            idx -= map.GetOffices().size();
        }
        return Item{{0.0, 0.0}, 0.0};
    }
    size_t GatherersCount() const override {
        return players_.GetPlayers().size();
    }
    Gatherer GetGatherer(size_t idx) const override {
        const auto& end = players_.GetPlayers().at(idx)->GetDog().GetPosition();
        const auto& start = players_.GetPlayers().at(idx)->GetDog().GetStartPosition();
        double width = 0.6;
        return Gatherer({start.x, start.y}, {end.x, end.y}, width);
    }
private:
    model::Game& game_;
    players::Players& players_;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector
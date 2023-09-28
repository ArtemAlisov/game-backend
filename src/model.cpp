#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(const Office& office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(const Map& map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}


void Game::AddJsonMap(const Map::Id& id, const std::string& json_string) {
    maps_to_json_[id] = json_string;
}



bool Dog::TakeLoot(std::shared_ptr<LootObject> loot) {
    if(bag_.size() < bag_capacity_ && loot) {
        bag_.push_back(std::make_shared<LootObject>(*loot));
        return true;
    }
    return false;
}

void Dog::SetDirection(const std::string& direction) {
    if (direction == "L") {
        speed_.x = -nominal_speed_;
        speed_.y = 0;
    }
    else if (direction == "R"){
        speed_.x = nominal_speed_;
        speed_.y = 0;
    }
    else if (direction == "U"){
        speed_.x = 0;
        speed_.y = -nominal_speed_;
    }
    else if (direction == "D"){
        speed_.x = 0;
        speed_.y = nominal_speed_;
    }
    else{
        speed_.x = 0;
        speed_.y = 0;
    }
    dir_ = direction;
}

std::vector<std::shared_ptr<LootObject>> Dog::ReturnLoot() {
    std::vector<std::shared_ptr<LootObject>> returned_loot = bag_;
    bag_ = {};
    return returned_loot;
}

const Point GameSession::GetRandomLocation() {
    const model::Map::Roads roads = map_->GetRoads();
    int road_number = loot_gen::GetRandomItem(roads.size()-1);
    Road road = roads[road_number];
    Point start = road.GetStart();
    Point end = road.GetEnd();
    int x = std::min(start.x, end.x) + loot_gen::GetRandomItem(std::abs(start.x - end.x));
    int y = std::min(start.y, end.y) + loot_gen::GetRandomItem(std::abs(start.y - end.y));
    return Point{x, y}; 
}

std::shared_ptr<Dog> GameSession::AddDog(const std::string& name) {
    Point start = GetLocation();
    int dog_speed = map_->GetDogSpeed();
    dogs_.push_back(std::make_shared<Dog>(name, ids_++, dog_speed, start, timer_));
    dogs_.back()->SetBagCapacity(bag_capacity_);
    return dogs_.back();
}

void GameSession::AddNewLoot(std::shared_ptr<LootObject> loot_object, int id) {
    loot_object->SetPosition(GetLocation());
    loot_objects_[id] = loot_object;
}

void GameSession::DeleteLootObject(int id) {
    if(loot_objects_.contains(id)) 
        loot_objects_.erase(id);
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    auto it = map_id_to_index_.find(id);
    if (it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    
    return nullptr;
}

std::string Game::GetJsonMap(const Map::Id& id) const noexcept {
    if (maps_to_json_.contains(id)) {
        return maps_to_json_.at(id);
    }
    return "";
}

GameSession& Game::StartGameSession(const Map& map) {
    sessions_[map.GetId()] = GameSession(map, random_);
    sessions_[map.GetId()].SetBagCapacity(map.GetBagCapacity() ? map.GetBagCapacity() : bag_capacity_);
    sessions_[map.GetId()].RefreshTimer(timer_);
    return sessions_[map.GetId()];
}

void Game::GenerateLoot(const TimeInterval& time_interval) {
    for(auto& [map_id, session] : sessions_) {
        unsigned loot_count = session.GetLootObjects().size();
        unsigned looter_count = session.GetNumberOfPlayers();
        unsigned needed_loot = loot_generator_.Generate(time_interval, loot_count, looter_count);
        if(needed_loot) {
            for(unsigned i = 0; i < needed_loot; ++i) {
                int loot_max_type = session.GetMap().GetLootObjectNumber();
                int loot_type = loot_gen::GetRandomItem(loot_max_type);
                loot_objects_.push_back(std::make_shared<LootObject>(loot_number_++, loot_type));
                session.AddNewLoot(loot_objects_.back(), loot_objects_.back()->GetId());
            }
        }
    }
}

}  // namespace model

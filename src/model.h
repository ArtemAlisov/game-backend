#pragma once

#include <chrono>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
#include "util/tagged_uuid.h"

namespace model {

using namespace std::literals;
using Dimension = int;
using Coord = Dimension;

struct Point {
    const size_t operator()() {
        int64_t mod_1 = 1e3 + 9; // prime number
        int64_t mod_2 = 1e6 + 33;  // prime number

        // Calculate Hash from coordinates x and y. Expect (x and y) < 1000;
        return (x * mod_1 + y * mod_2) % mod_2; 
    }
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, int types) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , max_loot_info_type_(types) {
    }

    const int GetLootTypes() const noexcept {
        return max_loot_info_type_;
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(const Office& office);

    void SetDogSpeed(double dog_speed) {
        default_dog_speed_ = dog_speed;
    }

    void SetLootObjectNumber(int number) {
        max_loot_object_number_ = number;
    }

    int GetLootObjectNumber() const {
        return max_loot_object_number_;
    }

    double GetDogSpeed() {
        return default_dog_speed_;
    }

    void AddRoadMap(const Point point, const Road& road) {
        road_map_[point.x][point.y].insert(std::make_shared<Road>(road));
    }

    std::set<std::shared_ptr<Road>> GetRoadMap(const Point point) {
        return road_map_[point.x][point.y];
    }

    int GetBagCapacity() const {
        return bag_capacity_;
    }

    void SetBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double default_dog_speed_ = 1;
    int max_loot_info_type_ = 0;
    int bag_capacity_ = 0;
    Roads roads_;
    Buildings buildings_;
    std::unordered_map<int, std::unordered_map<int, std::set<std::shared_ptr<Road>>>> road_map_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    int max_loot_object_number_;
};


class LootObject;

namespace detail {
    struct DogTag {};
} // namespace detail

using DogId = util::TaggedUUID<detail::DogTag>;

class Dog {
public:
    using Bag = std::vector<std::shared_ptr<LootObject>>;

    Dog(const std::string& name, const int& id, const double& speed, const Point& coords, const double& start_time)
        : name_(name)
        , id_(id)
        , nominal_speed_(speed)
        , coords_({ coords.x, coords.y })
        , start_coords_({ coords.x, coords.y })
        , uuid_{DogId::New()}
    {
    }

    Dog(int id) 
        : id_(id) {
    }

    struct Coords {
        Coords() = default;

        template<typename T>
        Coords(T x, T y) 
            : x(x)
            , y(y)
        {
        }
        double x = 0;
        double y = 0;
    };
    struct Speed {
        Speed() = default;

        double x = 0;
        double y = 0;
    };

    const std::string& GetName() const noexcept {
        return name_;
    }

    void SetName(std::string name) {
        name_ = name;
    }

    int GetId() {
        return id_;
    }

    const Coords& GetPosition() const noexcept {
        return coords_;
    }

    const Coords& GetStartPosition() const noexcept {
        return start_coords_;
    }

    void SetPosition(double x, double y) {
        start_coords_ = {coords_.x, coords_.y};
        coords_.x = x;
        coords_.y = y;
    }

    const Speed& GetSpeed() const noexcept {
        return speed_;
    }

    void Stop() {
        speed_.x = 0;
        speed_.y = 0;
    }

    const std::string& GetDirection() const noexcept {
        return dir_;
    }

    void SetDirection(const std::string& direction);

    bool TakeLoot(std::shared_ptr<LootObject> loot);

    const int& GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

    std::vector<std::shared_ptr<LootObject>> ReturnLoot();

    double GetRetirementTime() {
        if(std::abs(speed_.x - speed_.y) > std::numeric_limits<double>::epsilon()) {
            last_activity_ = current_time_;
        }

        return current_time_ - last_activity_;
    }

    void RefreshTime(double time) {
        current_time_ += time;
    }

    const DogId& GetUUID() const noexcept {
        return uuid_;
    }

    void SetUUID(const std::string& new_uuid) noexcept {
        *uuid_ = util::detail::UUIDFromString(new_uuid);
    }

    const double& GetStartTime() const noexcept {
        return start_time_;
    }

    void SetStartTime(const double& time) {
        start_time_ = time;
    }

    const double& GetCurrentTime() const noexcept {
        return current_time_;
    }

    void SetCurrentTime(const double& time) {
        current_time_ = time;
    }

    const double& GetNominalSpeed() const noexcept {
        return nominal_speed_;
    }

    const int& GetId() const noexcept {
        return id_;
    }

    const Bag& GetBag() const noexcept {
        return bag_;
    }

    const double GetActivityTime() const noexcept {
        return last_activity_;
    }

    void SetActivityTime(const double& time) {
        last_activity_ = time;
    }

    void SetCoords(const Coords& coords) {
        coords_ = coords;
    }

    void SetStartCoords(const Coords& coords) {
        start_coords_ = coords;
    }

    void SetNominalSpeed(const double& speed) {
        nominal_speed_ = speed;
    }

private:

    std::string name_;
    int id_ = 0;
    double nominal_speed_ = 0.0;
    Coords coords_;
    Coords start_coords_;
    Speed speed_;
    std::string dir_ = "U";
    Bag bag_ = {};
    int bag_capacity_ = 0;
    double last_activity_ = 0.0;
    double start_time_ = 0.0;
    double current_time_ = 0.0;
    DogId uuid_;
};


class LootObject {
public:
    explicit LootObject(int id) 
            : id_(id)
        {
        }

    LootObject(int id, int type) 
            : id_(id)
            , type_(type)
    {
    }

    LootObject(int id, double x, double y) 
            : id_(id)
            , coords_{std::move(x), std::move(y)} 
            {
            }

    const int GetId() const {
        return id_;
    }

    const int GetType() const {
        return type_;
    }

    const model::Dog::Coords& GetPosition() const noexcept {
        return coords_;
    }

    void SetPosition(Point location) {
        coords_ = {location.x, location.y};
    }

    void SetPosition(model::Dog::Coords location) {
        coords_ = location;
    }

    void SetInvisible() {
        visible_ = false;
    }

    const bool& IsVisible() const noexcept {
        return visible_;
    }

private:
    int id_ = 0;
    int type_ = 0;
    model::Dog::Coords coords_{0, 0};
    bool visible_ = true;
};

class GameSession {
public:
    GameSession() {

    }

    GameSession(const Map& map, bool random) 
        : map_(std::make_shared<Map>(map))
        , dogs_{}
        , ids_(0)
        , random_(random)
    {
    }

    Map& GetMap() {
        return *map_;
    }

    const Point GetRandomLocation();

    const Point GetLocation() {
        return random_ ? GetRandomLocation() : map_->GetRoads().front().GetStart();
    }

    std::shared_ptr<Dog> AddDog(const std::string& name);

    void AddNewLoot(std::shared_ptr<LootObject> loot_object, int id);

    const std::unordered_map<int, std::shared_ptr<LootObject>>& GetLootObjects() const {
        return loot_objects_;
    }

    std::unordered_map<int, std::shared_ptr<LootObject>>& GetLootObjects() {
        return loot_objects_;
    }

    unsigned GetNumberOfPlayers() {
        return dogs_.size() - retired_;
    }

    void AddRetiredOne() {
        ++retired_;
    }

    std::vector<std::shared_ptr<Dog>>& GetDogs() {
        return dogs_;
    }

    void DeleteLootObject(int id);

    void SetBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

    void RefreshTimer(const double& timer) {
        timer_ = timer;
    }

    const double GetTime() const noexcept {
        return timer_;
    }

    const int& GetRetired() const noexcept {
        return retired_;
    }

    bool GetRandom() const noexcept {
        return random_;
    }

    const int& GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetRandomRegime(bool random) {
        random_ = random;
    }

    void SetRetiredNumber(int retired) {
        retired_ = retired;
    }

private:
    std::shared_ptr<Map> map_;
    std::vector<std::shared_ptr<Dog>> dogs_;
    int ids_ = 0;
    bool random_ = false;
    std::unordered_map<int, std::shared_ptr<LootObject>> loot_objects_{};
    int bag_capacity_ = 0;
    double timer_ = 0.0;
    int retired_ = 0;
};


class Game {
public:
    using Maps = std::vector<Map>;
    using TimeInterval = std::chrono::milliseconds;

    Game(TimeInterval period, double probability) 
        : loot_generator_(loot_gen::LootGenerator{period, probability})
        {
        }

    void AddMap(const Map& map);
    void AddJsonMap(const Map::Id& id, const std::string& json_string);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept;

    std::string GetJsonMap(const Map::Id& id) const noexcept;

    GameSession& StartGameSession(const Map& map);

    GameSession& GetGameSession(const Map& map) {
        return HasGameSession(map) ? sessions_[map.GetId()] : StartGameSession(map);
    }

    const auto& GetGameSessions() const {
        return sessions_;
    }

    auto& GetGameSessions() {
        return sessions_;
    }

    bool HasGameSession(const Map& map) const {
        return sessions_.contains(map.GetId());
    }

    void GenerateLoot(const TimeInterval& time_interval);

    void SetLootObjects(std::vector<std::shared_ptr<LootObject>>& loot_objects) {
        loot_objects_ = loot_objects;
    }

    const std::vector<std::shared_ptr<LootObject>> GetLootObjects() const {
        return loot_objects_;
    }

    std::vector<std::shared_ptr<LootObject>>& GetLootObjects() {
        return loot_objects_;
    }

    void SetRandomMode() {
        random_ = true;
    }

    bool GetRandomMode() const noexcept {
        return random_;
    }

    void SetBagCapacity(int bag_capacity) {
        bag_capacity_ = bag_capacity;
    }

    const int& GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetRetirementTime(double dog_retirement_time) {
        dog_retirement_time_ = dog_retirement_time;
    }

    const double& GetRetirementTime() const noexcept {
        return dog_retirement_time_;
    }

    const double& GetTimer() const noexcept {
        return timer_;
    }

    void AddTime(const double& time_delta) {
        timer_ += time_delta;
    }

    const int& GetLootNumber() const noexcept {
        return loot_number_;
    }

    void SetLootNumber(int number) {
        loot_number_ = number;
    }

    void SetTimer(int timer) {
        timer_ = timer;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using timeInterval = std::chrono::milliseconds;

    loot_gen::LootGenerator loot_generator_;
    TimeInterval time_interval = 0ms;
    std::vector<Map> maps_{};
    MapIdToIndex map_id_to_index_{};
    std::unordered_map<Map::Id, std::string, MapIdHasher> maps_to_json_{};
    std::unordered_map<Map::Id, GameSession, MapIdHasher> sessions_{};
    double dog_retirement_time_ = 60.0;
    double timer_ = 0.0;
    int loot_number_ = 0;
	std::vector<std::shared_ptr<LootObject>> loot_objects_{};
    bool random_ = false;
    int bag_capacity_ = 3;
    
};


}  // namespace model
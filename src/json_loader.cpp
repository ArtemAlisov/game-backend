#include "json_loader.h"

#include <chrono>
#include <fstream>
#include <iostream>

using namespace std::literals;

namespace json_loader {

const std::string X1 = "x1"s;
const std::string X0 = "x0"s;
const std::string Y1 = "y1"s;
const std::string Y0 = "y0"s;
const std::string X = "x"s;
const std::string Y = "y"s;
const std::string W = "w"s;
const std::string H = "h"s;


void LoadRoad(json::value& road, model::Map& map_object) {
	if (road.as_object().if_contains(X1)) {
		int x0 = road.as_object().at(X0).as_int64();
		int y0 = road.as_object().at(Y0).as_int64();
		int x1 = road.as_object().at(X1).as_int64();
		model::Road road_object(model::Road::HORIZONTAL, { x0, y0 }, x1);

		while (x0 <= x1) {
			map_object.AddRoadMap({ x0, y0 }, road_object);
			++x0;
		}
		while (x1 <= x0) {
			map_object.AddRoadMap({ x1, y0 }, road_object);
			++x1;
		}

		map_object.AddRoad(road_object);
	}
	if (road.as_object().if_contains(Y1)) {
		int x0 = road.as_object().at(X0).as_int64();
		int y0 = road.as_object().at(Y0).as_int64();
		int y1 = road.as_object().at(Y1).as_int64();

		model::Road road_object(model::Road::VERTICAL, { x0, y0 }, y1);

		while (y0 <= y1) {
			map_object.AddRoadMap({ x0, y0 }, road_object);
			++y0;
		}
		while (y1 <= y0) {
			map_object.AddRoadMap({ x0, y1 }, road_object);
			++y1;
		}

		map_object.AddRoad(road_object);
	}
}

void LoadBuilding(json::value& building, model::Map& map_object) {
	int x = building.as_object().at(X).as_int64();
	int y = building.as_object().at(Y).as_int64();
	int w = building.as_object().at(W).as_int64();
	int h = building.as_object().at(H).as_int64();

	model::Building building_object({ {x, y}, {w, h} });
	map_object.AddBuilding(building_object);
}

void LoadOffice(json::value& office, model::Map& map_object) {
	int x = office.as_object().at(X).as_int64();
	int y = office.as_object().at(Y).as_int64();
	std::string id = office.as_object().at("id").as_string().data();
	int offsetX = office.as_object().at("offsetX").as_int64();
	int offsetY = office.as_object().at("offsetY").as_int64();

	model::Office office_object(util::Tagged<std::string, model::Office>(id), { x, y }, { offsetX, offsetY });
	map_object.AddOffice(office_object);
}

void LoadMap(json::value& map, model::Game& game, const float& speed) {
	std::string name = map.as_object().at("name"s).as_string().data();
	std::string id = map.as_object().at("id").as_string().data();
	int loot_types = map.as_object().at("lootTypes").as_array().size() - 1;
	model::Map map_object(util::Tagged < std::string, model::Map >(id), name, loot_types);
	map_object.SetLootObjectNumber(loot_types);

	for (auto& road : map.as_object().at("roads").as_array()) 
		LoadRoad(road, map_object);

	for (auto& building : map.as_object().at("buildings").as_array())
		LoadBuilding(building, map_object);

	for (auto& office : map.as_object().at("offices").as_array())
		LoadOffice(office, map_object);

	if (map.as_object().contains("dogSpeed")) {
		map_object.SetDogSpeed(map.as_object()["dogSpeed"].as_double());
		map.as_object().erase("dogSpeed");
	}
	else
		map_object.SetDogSpeed(speed);

	if(map.as_object().contains("bagCapacity"))	
		map_object.SetBagCapacity(map.as_object()["bagCapacity"].as_int64());

	game.AddMap(map_object);
	game.AddJsonMap(util::Tagged < std::string, model::Map >(id), json::serialize(map));
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    
    using TimeInterval = std::chrono::milliseconds;

    std::ifstream in(json_path);
    if (!in.is_open()) {
		try {
			in.open(json_path);
		}
		catch (...) {
			std::cout << "Couldn't read file..." << std::endl;
			throw;
		}
    }

    std::string game_data;
    std::string line;
    while (std::getline(in, line)) {
        game_data += line;
    }    

    auto parsed_data = json::parse(game_data.data());
	using Id = util::Tagged<std::string, model::Map>;
	double initial_speed = 1.0;

	auto loot_settings = parsed_data.as_object()["lootGeneratorConfig"s].as_object();
	TimeInterval period = std::chrono::duration_cast<TimeInterval>(loot_settings["period"s].as_double() * 1.0ms);
	double probability = loot_settings["probability"s].as_double();

	model::Game game(period, probability);
	if (parsed_data.as_object().contains("defaultDogSpeed"s))
		initial_speed = parsed_data.as_object()["defaultDogSpeed"s].as_double();
	if (parsed_data.as_object().contains("defaultBagCapacity"s)) 
		game.SetBagCapacity(parsed_data.as_object()["defaultBagCapacity"s].as_int64());
	if (parsed_data.as_object().contains("dogRetirementTime"s))
		game.SetRetirementTime(parsed_data.as_object()["dogRetirementTime"s].as_double());
	for (auto& map : parsed_data.as_object().at("maps"s).as_array()) 
		LoadMap(map, game, initial_speed);
	
		
    return std::move(game);
}

}  // namespace json_loader
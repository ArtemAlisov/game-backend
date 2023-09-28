#pragma once

#include <filesystem>

#include "model.h"

#include <boost/json.hpp>

namespace json = boost::json;

namespace json_loader {

void LoadRoad(json::value& road, model::Map& map_object);

void LoadBuilding(json::value& building, model::Map& map_object);

void LoadOffice(json::value& office, model::Map& map_object);

void LoadMap(json::value& map, model::Game& game, const float& speed);

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
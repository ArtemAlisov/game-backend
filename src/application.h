#pragma once

#include <boost/json.hpp>

#include "collision_detector.h"
#include "db_connection.h"
#include "model.h"
#include "player.h"
#include "serialization.h"

#include <chrono>
#include <cmath>

namespace json = boost::json;

namespace application {

	class Application {
	public:
		Application(model::Game& game, players::Players& players, players::PlayerTokens& tokens, conn_pool::ConnectionPool& conn_pool, int save_period, std::string save_path) 
            : game_(game)
            , players_(players)
            , player_tokens_(tokens)
            , conn_pool_(conn_pool)
            , save_period_(save_period)
            , save_path_(save_path)
        {
        }

        void Tick(std::chrono::milliseconds delta) {
            game_.GenerateLoot(delta);
            int time = delta.count();
            int msc_in_sec = 1000;            
            game_.AddTime(1.0 * time / msc_in_sec);
            const double timer = game_.GetTimer();
            
            if(!save_path_.empty() && save_period_ != 0 && prev_saving_ < timer * msc_in_sec - save_period_) {
                serializer::SerializeGame(save_path_, game_, players_, player_tokens_);
                prev_saving_ = timer * msc_in_sec;
            }

            const double retirement_time = game_.GetRetirementTime();
            for (auto& player : players_.GetPlayers()) {
                auto game_session = player->GetGameSession();
                game_session.RefreshTimer(timer);
                auto& dog = player->GetDog();
                dog.RefreshTime(1.0 * time / msc_in_sec);

                if(!player->IsOnline())
                    continue;

                auto position = dog.GetPosition();
                int x = std::round(position.x);
                int y = std::round(position.y);

                double speed_x = dog.GetSpeed().x;
                double speed_y = dog.GetSpeed().y;
                auto roads = game_session.GetMap().GetRoadMap({ x, y });
                bool needed_to_stop = false;

                double expected_x = position.x + speed_x * time / msc_in_sec;
                double current_x = position.x;
                double expected_y = position.y + speed_y * time / msc_in_sec;
                double current_y = position.y;

                const double ROAD_WIDTH = 0.8;
                if (speed_x - std::numeric_limits<double>::epsilon() > 0) {
                    for (const auto& road : roads) {
                        double possible_x_max = std::max(road->GetStart().x, road->GetEnd().x) * 1.0 + ROAD_WIDTH / 2;
                        if (expected_x > possible_x_max + std::numeric_limits<double>::epsilon() && expected_x > current_x + std::numeric_limits<double>::epsilon()) {
                            current_x = std::max(current_x, possible_x_max);
                            needed_to_stop = true;
                        }
                        else if (expected_x > current_x + std::numeric_limits<double>::epsilon()) {
                            current_x = expected_x;
                            needed_to_stop = false;
                        }
                    }
                }
                else if (speed_x + std::numeric_limits<double>::epsilon() < 0) {
                    for (const auto& road : roads) {
                        double possible_x_min = std::min(road->GetStart().x, road->GetEnd().x) * 1.0 - ROAD_WIDTH / 2;
                        if (expected_x < possible_x_min - std::numeric_limits<double>::epsilon() && expected_x < current_x - std::numeric_limits<double>::epsilon()) {
                            current_x = std::min(current_x, possible_x_min);
                            needed_to_stop = true;
                        }
                        else if (expected_x < current_x - std::numeric_limits<double>::epsilon()) {
                            current_x = expected_x;
                            needed_to_stop = false;
                        }
                    }
                }
                else if (speed_y - std::numeric_limits<double>::epsilon() > 0) {
                    for (const auto& road : roads) {
                        double possible_y_max = std::max(road->GetStart().y, road->GetEnd().y) * 1.0 + ROAD_WIDTH / 2;
                        if (expected_y > possible_y_max + std::numeric_limits<double>::epsilon() && expected_y > current_y + std::numeric_limits<double>::epsilon()) {
                            current_y = std::max(current_y, possible_y_max);
                            needed_to_stop = true;
                        }
                        else if (expected_y > current_y + std::numeric_limits<double>::epsilon()) {
                            current_y = expected_y;
                            needed_to_stop = false;
                        }
                    }
                }
                else if (speed_y + std::numeric_limits<double>::epsilon() < 0) {
                    for (const auto& road : roads) {
                        double possible_y_min = std::min(road->GetStart().y, road->GetEnd().y) * 1.0 - ROAD_WIDTH / 2;
                        if (expected_y < possible_y_min - std::numeric_limits<double>::epsilon() && expected_y < current_y - std::numeric_limits<double>::epsilon()) {
                            current_y = std::min(current_y, possible_y_min);
                            needed_to_stop = true;
                        }
                        else if (expected_y < current_y - std::numeric_limits<double>::epsilon()) {
                            current_y = expected_y;
                            needed_to_stop = false;
                        }
                    }
                }

                if(dog.GetRetirementTime() > retirement_time || std::abs(dog.GetRetirementTime() - retirement_time) < std::numeric_limits<double>::epsilon()) {
                    player->SetOffline();
                    game_session.AddRetiredOne();
                    {
                        auto conn = conn_pool_.GetConnection();
                        pqxx::work work{*conn};
                        double total_time = (dog.GetCurrentTime() - dog.GetStartTime());
                        work.exec_params("INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4)"_zv, 
                            dog.GetUUID().ToString(), dog.GetName(), player->GetValue(), total_time);
                        work.commit();
                    }
                }

                if (needed_to_stop)
                    dog.Stop();
                player->GetDog().SetPosition(current_x, current_y);
            }

            collision_detector::LootGathererProvider provider(game_, players_);
            std::vector<collision_detector::GatheringEvent> events = FindGatherEvents(provider);
            for(const auto& event : events) {
                int item_id = game_.GetLootObjects().at(event.item_id)->GetId();
                for(auto& game_session : game_.GetGameSessions()) {
                    if(!game_session.second.GetLootObjects().contains(item_id)) {
                        continue;  //here was "break"
                    }
                    auto& loot = game_session.second.GetLootObjects()[item_id];
                    if(players_.GetPlayers()[event.gatherer_id]->GetDog().TakeLoot(loot)) {
                        game_.GetLootObjects().at(event.item_id)->SetInvisible();
                        game_session.second.DeleteLootObject(item_id);
                    }
                    
                    break;
                }
            }

            collision_detector::OfficeGathererProvider office_provider(game_, players_);
            std::vector<collision_detector::GatheringEvent> office_events = FindGatherEvents(office_provider);

            for(const auto& event : office_events) {
                for(auto& game_session : game_.GetGameSessions()) {
                    if(game_session.second.GetDogs().size() != players_.GetPlayers()[event.gatherer_id]->GetGameSession().GetDogs().size()) {
                        continue;
                    }
                    model::Dog& dog = players_.GetPlayers()[event.gatherer_id]->GetDog();
                    for(auto& loot : dog.ReturnLoot()) {
                        json::value parsed_game_data = json::parse(game_.GetJsonMap(game_session.second.GetMap().GetId()));
                        int loot_value = parsed_game_data.as_object()["lootTypes"].as_array()[loot->GetType()].as_object()["value"].as_int64();
                        players_.GetPlayers()[event.gatherer_id]->AddValue(loot_value);
                    }
                }

                break;
            }

        }

	private:
		model::Game& game_;
		players::Players& players_;
        players::PlayerTokens& player_tokens_;
        conn_pool::ConnectionPool& conn_pool_;
        int save_period_ = 0;
        int prev_saving_ = 100;
        std::string save_path_;
	};
}
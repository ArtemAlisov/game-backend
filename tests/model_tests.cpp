#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "../src/model.h"

using namespace std::literals;

SCENARIO("Dogs generation") {
    using namespace model;

    GIVEN("a game") {
        Game game(1s, 1.0);

        Road road(Road::HORIZONTAL, {0, 0}, 100);
        
        Map::Id map_id{"map1"};
        Map map(map_id, "Map 1", 1);
        map.AddRoad(road);
        game.AddMap(map);
        game.SetRandomMode();

        WHEN("start a new game session") {
            GameSession game_session = game.StartGameSession(map);
            
            THEN("game session is generated") {
                REQUIRE(*game_session.GetMap().GetId() == *game.GetGameSession(map).GetMap().GetId());
            }
            
            WHEN("add new dog") {
                THEN("dog is generated") {
                    for(int i = 1; i <= 10; ++i) {
                        game_session.AddDog("name");
                        int dogs_count = game_session.GetDogs().size();
                        int loot_count = game_session.GetLootObjects().size();
                        INFO("dog count" << dogs_count << ", dogs added: " << i); 
                        REQUIRE(dogs_count == i);
                    }
                }
                
                game_session.AddDog("name");
                WHEN("set random mode") {
                    THEN("dog located not in 0-point") {
                        double x = game_session.GetDogs().front()->GetPosition().x;
                        double y = game_session.GetDogs().front()->GetPosition().y;
                        CHECK((x != 0. || y != 0.));
                    }
                }
            }
        }
    }
}
#pragma once

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#include "model.h"
#include "player.h"

#include <filesystem>
#include <fstream>

namespace model {

    template <typename Archive>
    void serialize(Archive& ar, Point& point, [[maybe_unused]] const unsigned version) {
        ar& point.x;
        ar& point.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Dog::Speed& speed, [[maybe_unused]] const unsigned version) {
        ar& speed.x;
        ar& speed.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Dog::Coords& speed, [[maybe_unused]] const unsigned version) {
        ar& speed.x;
        ar& speed.y;
    }
    
    class LootSerializer {
    public:
        LootSerializer() = default;

        explicit LootSerializer(const LootObject& loot) 
            : id_(loot.GetId())
            , type_(loot.GetType())
            , coords_(loot.GetPosition())
            , visible_(loot.IsVisible()) {
        }

        [[nodiscard]] LootObject Restore() const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& id_;
            ar& type_;
            ar& coords_;
            ar& visible_;
        }

    private:
        int id_;
        int type_;
        Dog::Coords coords_;
        bool visible_;
    };

    class DogSerializer {
    public:
        DogSerializer() = default;

        using Bag = std::vector<std::shared_ptr<LootObject>>;

        explicit DogSerializer(const Dog& dog) 
                : name_(dog.GetName()) 
                , nominal_speed_(dog.GetNominalSpeed())
                , coords_(dog.GetPosition())
                , start_coords_(dog.GetStartPosition())
                , speed_(dog.GetSpeed())
                , dir_(dog.GetDirection())
                , bag_capacity_(dog.GetBagCapacity()) 
                , last_activity_(dog.GetActivityTime())
                , start_time_(dog.GetStartTime())
                , current_time_(dog.GetCurrentTime())
                , uuid_(dog.GetUUID().ToString()) {

            for(const auto& loot : dog.GetBag()) {
                loot_ids_.push_back(loot->GetId());
            }
        }

        void Restore(Dog& dog, std::vector<std::shared_ptr<LootObject>> loot_objects) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& name_;
            ar& id_;
            ar& nominal_speed_;
            ar& coords_;
            ar& start_coords_;
            ar& speed_;
            ar& dir_;
            ar& bag_capacity_;
            ar& last_activity_;
            ar& start_time_;
            ar& current_time_;
            ar& uuid_;
            ar& loot_ids_;
        }

    private:
        std::string name_;
        int id_;
        double nominal_speed_;
        Dog::Coords coords_;
        Dog::Coords start_coords_;
        Dog::Speed speed_;
        std::string dir_ = "U";
        int bag_capacity_ = 0;
        double last_activity_ = 0.0;
        double start_time_ = 0.0;
        double current_time_ = 0.0;
        std::string uuid_;
        std::vector<int> loot_ids_;
    };

    class GameSessionSerializer {
    public:
        GameSessionSerializer() = default;

        explicit GameSessionSerializer(const GameSession& game_session) 
                : retired_(game_session.GetRetired()) {

            for(const auto& loot : game_session.GetLootObjects()) 
                loot_ids_.push_back(loot.second->GetId());
        }

        void Restore(GameSession& game_session, const std::vector<std::shared_ptr<LootObject>> loot_objects) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& retired_;
            ar& loot_ids_;
        }

    private:
        int retired_;
        std::vector<int> loot_ids_;
    };

    class GameSerializer {
    public:
        GameSerializer() = default;

        using Bag = std::vector<std::shared_ptr<LootObject>>;

        explicit GameSerializer(const Game& game) 
                : timer_(game.GetTimer())
                , loot_number_(game.GetLootNumber()) {

            for(const auto& loot : game.GetLootObjects())
                loot_objects_.push_back(LootSerializer(*loot));
        }

        void Restore(Game& game) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& timer_;
            ar& loot_number_;
            ar& loot_objects_;
        }

    private:
        double timer_;
        int loot_number_;
        std::vector<LootSerializer> loot_objects_;
    };
} // namespace model 

namespace players {
    class PlayerSerializer {
    public:
        PlayerSerializer() = default;
        PlayerSerializer(const Player& player) 
                : map_id_(player.GetMapId())
                , name_(player.GetName())
                , value_(player.GetValue())
                , online_(player.IsOnline()) {
        }

        void Restore(Players& players, model::Game& game) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_id_;
            ar& name_;
            ar& value_;
            ar& online_;
        }

    private:
        std::string map_id_;
        std::string name_;
        int value_;
        bool online_;
    };

    class TokensSerializer {
    public:
        TokensSerializer() = default;

        TokensSerializer(const PlayerTokens& tokens) {
            for(auto token : tokens.GetTokens()) {
                tokens_.push_back(*token);
                if(tokens.GetPlayerByToken(token)) 
                    players_.push_back(tokens.GetPlayerByToken(token)->GetId());
            }
        }

        void Restore(PlayerTokens& tokens, const std::vector<std::shared_ptr<Player>>& players) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& tokens_;
            ar& players_;
        }

    private:
        std::vector<std::string> tokens_;
        std::vector<int> players_;
    };
} // namespace players



namespace serializer {

    class ApplicationSerializer {
    public:
        ApplicationSerializer() = default;
        
        explicit ApplicationSerializer(const model::Game& game, const players::Players& players, const players::PlayerTokens& tokens)
                    : game_(model::GameSerializer(game))
                    , tokens_(players::TokensSerializer(tokens)) {
                
                for(const auto& player : players.GetConstPlayers()) 
                    players_.push_back(players::PlayerSerializer(*player));

                for(const auto& map : game.GetMaps()) {
                    model::Map::Id map_id = map.GetId();
                    if(game.GetGameSessions().contains(map_id)) 
                        game_sessions_.push_back(model::GameSessionSerializer(game.GetGameSessions().at(map_id)));
                }

                for(const auto& player : players.GetConstPlayers()) {
                    dogs_.push_back(model::DogSerializer(player->GetDog()));
                }
            }
        
        void Restore(model::Game& game, players::Players& players, players::PlayerTokens& tokens) const;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& game_;
            ar& tokens_;
            ar& players_;
            ar& game_sessions_;
            ar& dogs_;
        }

    private:
        model::GameSerializer game_;
        players::TokensSerializer tokens_;
        std::vector<players::PlayerSerializer> players_;
        std::vector<model::GameSessionSerializer> game_sessions_;
        std::vector<model::DogSerializer> dogs_;
    };  

    void SerializeGame(const std::string& path, const model::Game& game, 
                        const players::Players& players, const players::PlayerTokens& tokens);

    void DeserializeGame(std::string& path, model::Game& game, 
                            players::Players& players, players::PlayerTokens& tokens);
} // namespace serializer
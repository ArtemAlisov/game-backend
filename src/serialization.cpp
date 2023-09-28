#include "serialization.h"

namespace model {
    [[nodiscard]] LootObject LootSerializer::Restore() const {
        LootObject loot{id_, type_};
        loot.SetPosition(coords_);
        if(!visible_) 
            loot.SetInvisible();
        return loot;
    }


    void DogSerializer::Restore(Dog& dog, std::vector<std::shared_ptr<LootObject>> loot_objects) const {
        dog.SetName(name_);
        dog.SetCoords(coords_);
        dog.SetStartCoords(start_coords_);
        dog.SetNominalSpeed(nominal_speed_);
        dog.SetDirection(dir_);
        dog.SetBagCapacity(bag_capacity_);
        dog.SetActivityTime(last_activity_);
        dog.SetStartTime(start_time_);
        dog.SetCurrentTime(current_time_);
        dog.SetUUID(uuid_);
        for(const auto& loot : loot_ids_) {
            dog.TakeLoot(loot_objects[loot]);
        }
    }


    void GameSessionSerializer::Restore(GameSession& game_session, const std::vector<std::shared_ptr<LootObject>> loot_objects) const {
        game_session.SetRetiredNumber(retired_);

        for(int id : loot_ids_)
            game_session.AddNewLoot(loot_objects[id], id);
    }


    void GameSerializer::Restore(Game& game) const {
        game.SetTimer(timer_);
        game.SetLootNumber(loot_number_);

        std::vector<std::shared_ptr<LootObject>> loot_objects;
        for(const auto& loot : loot_objects_) {
            loot_objects.push_back(std::make_shared<LootObject>(loot.Restore()));
        }
        game.SetLootObjects(loot_objects);
    }
}  // namespace model

namespace players {


    void PlayerSerializer::Restore(Players& players, model::Game& game) const {
        model::Map::Id map_id(map_id_);
        model::GameSession& game_session = game.GetGameSession(*game.FindMap(map_id));
        std::shared_ptr<model::Dog> dog = game_session.AddDog(name_);
        players.Add(dog, game_session);
        auto player = players.GetPlayers().back();
        player->AddValue(value_);
        if(!online_) 
            player->SetOffline();
    }

    void TokensSerializer::Restore(PlayerTokens& tokens, const std::vector<std::shared_ptr<Player>>& players) const {
        for(int i = 0; i < tokens_.size(); ++i) {
            Token token{tokens_[i]};
            tokens.AddPlayerWithToken(token, players[players_[i]]);
        }
    }

}  // namespace players
namespace serializer {

        
    void ApplicationSerializer::Restore(model::Game& game, players::Players& players, players::PlayerTokens& tokens) const {
        game_.Restore(game);
        for(const auto& player: players_) {
            player.Restore(players, game);
        }
        
        for(const auto& map : game.GetMaps()) {
            model::Map::Id map_id = map.GetId();
            int id = 0;
            if(game.GetGameSessions().contains(map_id))
                game_sessions_[id++].Restore(game.GetGameSessions()[map_id], game.GetLootObjects());
        }

        for(size_t i = 0; i < players.GetPlayers().size(); i++) {
            dogs_[i].Restore(players.GetPlayers()[i]->GetDog(), game.GetLootObjects());
        }
        
        tokens_.Restore(tokens, players.GetPlayers());
    }

    void SerializeGame(const std::string& path, const model::Game& game, 
                        const players::Players& players, const players::PlayerTokens& tokens) {
        std::string tmp_path = path + "_tmp";
        std::ofstream out(tmp_path, std::ios_base::binary);
        boost::archive::binary_oarchive ar{out};
        ApplicationSerializer app_serializer(game, players, tokens);
        ar << app_serializer;
        std::filesystem::rename(tmp_path, path);
    }

    void DeserializeGame(std::string& path, model::Game& game, 
                            players::Players& players, players::PlayerTokens& tokens) {
        if(!std::filesystem::exists(path))
            return;
        std::ifstream in(path, std::ios_base::binary);
        boost::archive::binary_iarchive ar{in};
        ApplicationSerializer app_serializer(game, players, tokens);
        ar >> app_serializer;
        app_serializer.Restore(game, players, tokens);
    }

} // namespace serializer
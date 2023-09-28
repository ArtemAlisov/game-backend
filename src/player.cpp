#include "player.h"

namespace players {

        const int& Player::GetId() const noexcept {
			return id_;
		}

		const std::string& Player::GetName()  const noexcept {
			return dog_->GetName();
		}

		model::Dog& Player::GetDog() {
			return *dog_;
		}

		const model::Dog& Player::GetDog() const noexcept {
			return *dog_;
		}

		model::GameSession& Player::GetGameSession() {
			return *session_;
		}

		void Player::AddValue(int value) {
			value_ += value;
		}

		const int& Player::GetValue() const noexcept {
			return value_;
		}

        std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(Token& token) {
			return token_to_player_.contains(token) ? token_to_player_[token] : nullptr;
		}

		Token& PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
			Token token(GenerateToken());
			while (token_to_player_.contains(token))
				*token = GenerateToken();
			token_to_player_[token] = player;
			tokens_.push_back(token);
			return tokens_.back();
		}

        std::string PlayerTokens::GenerateToken() {
			std::stringstream stream;
			stream << std::hex << generator1_() << std::hex << generator2_();
			while (stream.str().size() < 32)
				stream << std::hex << 0;
			return std::string(stream.str());
		}

        std::shared_ptr<Player> Players::Add(std::shared_ptr<model::Dog> dog, model::GameSession& session) {
			players_.push_back(std::make_shared<Player>(session, dog, ids_++));
			players_by_dogid_mapid_[dog->GetId()][session.GetMap().GetId()] = players_.back();
			return players_.back();
		}
		
		std::vector<std::shared_ptr<Player>>& Players::GetPlayers() {
			return players_;
		}

} // namespace players
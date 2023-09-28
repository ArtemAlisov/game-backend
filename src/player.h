#pragma once

#include "model.h"

#include <memory>
#include <random>
#include <sstream>
#include <unordered_map>

namespace detail {
	struct TokenTag {};
} // namespace detail

namespace players {

	class Player {
	public:
		Player(model::GameSession& session, std::shared_ptr<model::Dog> dog, int id, bool online=true) 
			: session_(std::make_shared<model::GameSession>(session))
			, dog_(dog)
			, id_(id)
			, online_(online)
		{
		}

		const int& GetId() const noexcept;
		const std::string& GetName() const noexcept;
		model::Dog& GetDog();
		const model::Dog& GetDog() const noexcept;
		model::GameSession& GetGameSession();
		void AddValue(int value);
		const int& GetValue() const noexcept;
		void SetOffline() {
			online_ = false;
		}
		const bool& IsOnline() const noexcept {
			return online_;
		}
		const std::string& GetMapId() const noexcept {
			return *session_->GetMap().GetId();
		}

	private:
		std::shared_ptr<model::GameSession> session_;
		std::shared_ptr<model::Dog> dog_;
		int id_;
		int value_ = 0;
		bool online_ = true;
	};

	using Token = util::Tagged<std::string, detail::TokenTag>;

	class PlayerTokens {
	public:
		std::shared_ptr<Player> FindPlayerByToken(Token& token);
		Token& AddPlayer(std::shared_ptr<Player> player);
		const std::vector<Token> GetTokens() const noexcept {
			return tokens_;
		}
		void AddPlayerWithToken(Token token, std::shared_ptr<Player> player) {
			tokens_.push_back(std::move(token));
			token_to_player_[tokens_.back()] = player;
		}
		const std::shared_ptr<Player> GetPlayerByToken(const Token& token) const noexcept {
			return token_to_player_.contains(token) ? token_to_player_.at(token) : nullptr;
		}

	private:
		using TokenHasher = util::TaggedHasher<Token>;
		std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher> token_to_player_;
		std::vector<Token> tokens_;
		std::random_device random_device_;
		std::mt19937_64 generator1_{[this] {
									std::uniform_int_distribution < std::mt19937_64::result_type> dist;
									return dist(random_device_);
								}() };
		std::mt19937_64 generator2_{[this] {
									std::uniform_int_distribution<std::mt19937_64::result_type> dist;
									return dist(random_device_);
								}() };

		std::string GenerateToken();
	};

	class Players {
	public:
		Players() 
		{
		}

		std::shared_ptr<Player> Add(std::shared_ptr<model::Dog> dog, model::GameSession& session);
		std::vector<std::shared_ptr<Player>>& GetPlayers();
		const std::vector<std::shared_ptr<Player>>& GetConstPlayers() const noexcept {
			return players_;
		}

		const int& GetIds() const noexcept {
			return ids_;
		}

	private:
		int ids_ = 0;
		using MapIdHasher = util::TaggedHasher<model::Map::Id>;
		std::vector<std::shared_ptr<Player>> players_;
		std::unordered_map<int, std::unordered_map<model::Map::Id, std::shared_ptr<Player>, MapIdHasher>> players_by_dogid_mapid_;
	};

} // namespace player
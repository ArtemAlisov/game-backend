#include "request_handler.h"
#include "collision_detector.h"
#include "serialization.h"

#include <algorithm>
#include <iostream> 

namespace http_handler {

    namespace sys = boost::system;

    using FileRequestResult = std::variant<EmptyResponse, StringResponse, FileResponse>;
    FileRequestResult RequestHandler::HandleFileRequest(const StringRequest& req) {
        FileResponse res;
        res.version(req.version());
        std::string file{req.target().substr(1, req.target().size() - 1)};
        fs::path rel_path{req.target().substr(1, req.target().size() - 1)};
        fs::path base = fs::weakly_canonical(root_);
        fs::path target_path = fs::weakly_canonical(base / rel_path);

        if (IsSubPath(target_path)) {
            http::file_body::value_type file;
            if (fs::exists(target_path) && req.target() != "/"sv) {
                if (sys::error_code ec; file.open(target_path.string().data(), beast::file_mode::read, ec), ec) {
                    std::cout << "I couldn't open file: "s << target_path << std::endl;
                }
                res.result(http::status::ok);
            }
            else if (req.target() == "/"sv) {
                sys::error_code ec;
                target_path = fs::weakly_canonical(base / "index.html"s);
                file.open(target_path.string().data(), beast::file_mode::read, ec);
                res.result(http::status::ok);
            }
            else {
                sys::error_code ec;
                target_path = fs::weakly_canonical(base / "NOT_FOUND.txt"s);
                file.open(target_path.string().data(), beast::file_mode::read, ec);
                res.result(http::status::not_found);
            }
            res.body() = std::move(file);
            res.insert(http::field::content_type, GetFileType(target_path.string()));
            res.prepare_payload();
        }

        content_type_ = res[http::field::content_type];
        status_ = res.result_int();

        return res;
    }

    StringResponse RequestHandler::ReportServerError(unsigned version, bool keep_alive) {
        StringResponse response(http::status::internal_server_error, version);
        response.set(http::field::content_type, "application / json"s);
        response.body() = "{\"server error\"}"s;
        response.content_length(16);
        response.keep_alive(keep_alive);

        content_type_ = response[http::field::content_type];
        status_ = response.result_int();

        return response;
    }


    StringResponse RequestHandler::MakeStringResponse(http::status status, std::string_view text, unsigned version, bool keep_alive, std::string_view content_type) const {
        StringResponse response(status, version);
        response.set(http::field::content_type, content_type);
        response.body() = text;
        response.content_length(text.size());
        response.set(http::field::cache_control, "no-cache"s);
        response.keep_alive(keep_alive);

        return response;
    }

    StringResponse RequestHandler::MakeStringResponseAllowed(http::status status, std::string_view text, unsigned version, bool keep_alive, std::string_view content_type, std::string allowed) const {
        StringResponse response(status, version);
        response.set(http::field::allow, allowed);
        response.set(http::field::content_type, content_type);
        response.body() = text;
        response.content_length(text.size());
        response.set(http::field::cache_control, "no-cache"s);
        response.keep_alive(keep_alive);

        return response;
    }

    StringResponse RequestHandler::HandleApiRequest(const StringRequest& request) {
        auto stop = request.target().find_first_of('?');
        std::string api_request = std::string(request.target().substr(0, stop));
        StringResponse response;
        if (api_request == "/api/v1/maps"s)
            response = HandleApiRequestGetMaps(request);
        else if (api_request.substr(0, 13) == "/api/v1/maps/"s)
            response = HandleApiRequestGetMap(request);
        else if (api_request == "/api/v1/game/join"s)
            response = HandleApiRequestJoinGame(request);
        else if (api_request == "/api/v1/game/players"s)
            response = HandleApiRequestGetPlayers(request);
        else if (api_request == "/api/v1/game/state"s)
            response = HandleApiRequestGameState(request);
        else if (api_request == "/api/v1/game/player/action"s)
            response = HandleApiRequestGamePlayerAction(request);
        else if (api_request == "/api/v1/game/tick"s && tick_period_ == 0)
            response = HandleApiRequestGameTick(request);
        else if (api_request == "/api/v1/game/records"s)
            response = HandleApiRequestGameRecords(request);
        else 
            response = MakeStringError(http::status::bad_request, request.version());

        content_type_ = response[http::field::content_type];
        status_ = response.result_int();

        return response;
    }

    StringResponse RequestHandler::HandleApiRequestJoinGame(const StringRequest& request) {
        json::object json_response;
        std::string user_name;
        std::string mapId;
        if (request.method() != http::verb::post) {
            json_response["code"s] = "invalidMethod"s;
            json_response["message"s] = "Only POST method is expected"s;
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "POST"s);
        }

        try {
            json::value json_body = json::parse(request.body());
            if (!json_body.as_object().contains("userName"s) || !json_body.as_object().contains("mapId")) {
                json_response["code"s] = "invalidArgument"s;
                json_response["message"s] = "Join game request parse error"s;
                return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            user_name = json_body.as_object()["userName"s].as_string().data();
            mapId = json_body.as_object()["mapId"s].as_string().data();
        }
        catch (...) {
            json_response["code"s] = "invalidArgument"s;
            json_response["message"s] = "Join game request parse error"s;
            return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }
        
        model::Map::Id map_Id(mapId);
        if (user_name.empty()) {
            json_response["code"s] = "invalidArgument"s;
            json_response["message"s] = "Invalid name"s;
            return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        } else if (!game_.FindMap(map_Id)) {
            json_response["code"s] = "mapNotFound"s;
            json_response["message"s] = "Map not found"s;
            return MakeStringResponse(http::status::not_found, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }

        model::GameSession& game_session = game_.GetGameSession(*game_.FindMap(map_Id));
        std::shared_ptr<model::Dog> dog = game_session.AddDog(user_name);
        
        players_.Add(dog, game_session);
        players::Token& token = player_tokens_.AddPlayer(players_.GetPlayers().back());
        json_response["authToken"s] = *token;
        json_response["playerId"s] = players_.GetPlayers().back()->GetId();
        
        return MakeStringResponse(http::status::ok, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGetMaps(const StringRequest& request) const {
        std::string text = ConvertMapsToString();
        return MakeStringResponse(http::status::ok, text.data(), request.version(), request.keep_alive(), "application/json"sv);
    }
    
    StringResponse RequestHandler::HandleApiRequestGetMap(const StringRequest& request) const {
        auto start = request.target().find_last_of('/') + 1;
        auto map_id = request.target().substr(start);
        auto id = util::Tagged<std::string, model::Map>({ map_id.data(), map_id.size() });
        std::string target = game_.GetJsonMap(id);
        if (!(request.method() == http::verb::head || request.method() == http::verb::get)) {
            json::object json_response;
            json_response["code"s] = "invalidMethod"s;
            json_response["message"] = "Invalid method";
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "GET, HEAD"s);
        }
        if (target.empty()) {
            json::object json_response;
            json_response["code"] = "mapNotFound"s;
            json_response["message"s] = "Map not found"s;
            return MakeStringResponse(http::status::not_found, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }
        return MakeStringResponse(http::status::ok, target, request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGetPlayers(const StringRequest& request) const {
        json::object json_response;
        if (!(request.method() == http::verb::head || request.method() == http::verb::get)) {
            json_response["code"s] = "invalidMethod"s;
            json_response["message"] = "Invalid method";
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "GET, HEAD"s);
        }
        
        try {
            std::string request_token = { request[http::field::authorization].data(), request[http::field::authorization].size() };
            auto start = request_token.find_first_of(' ');
            if (start >= request_token.size() || !ValidToken(request_token)) {
                json_response["code"s] = "invalidToken"s;
                json_response["message"s] = "Authorization header is missing"s;
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            players::Token token(request_token.substr(start + 1));
            auto plr = player_tokens_.FindPlayerByToken(token);

            
            if (!plr || !plr->IsOnline()) {
                json_response["code"s] = "unknownToken"s;
                json_response["message"s] = "Player token has not been found"s;
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            json::object json_player;
            for (auto& player : players_.GetPlayers()) {
                json_player["name"s] = player->GetName();
                json_response[std::to_string(player->GetId())] = json_player;
            }
        }
        catch (...) {
            json_response["code"s] = "invalidToken"s;
            json_response["message"s] = "Authorization header is missing"s;
            return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }
        return MakeStringResponse(http::status::ok, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGameState(const StringRequest& request) const {
        json::object json_response;
        if (!(request.method() == http::verb::head || request.method() == http::verb::get)) {
            json_response["code"s] = "invalidMethod"s;
            json_response["message"s] = "Invalid method"s;
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "GET, HEAD"s);
        }

        try {
            std::string request_token = { request[http::field::authorization].data(), request[http::field::authorization].size() };
            auto start = request_token.find_first_of(' ');
            if (start >= request_token.size() || !ValidToken(request_token)) {
                json_response["code"s] = "invalidToken"s;
                json_response["message"s] = "Authorization header is required"s;
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            players::Token token(request_token.substr(start + 1));
            auto plr = player_tokens_.FindPlayerByToken(token);
            if (!plr || !plr->IsOnline()) {
                json_response["code"s] = "unknownToken"s;
                json_response["message"s] = "Player token has not been found"s;
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            json::object json_player;
            json::object json_info;
            for (auto& player : players_.GetPlayers()) {
                if(*plr->GetGameSession().GetMap().GetId() != *player->GetGameSession().GetMap().GetId())
                    continue;
                    
                model::Dog dog = player->GetDog();
                model::Dog::Coords coords = dog.GetPosition();
                json::array j_coords;
                j_coords.push_back(json::value(coords.x));
                j_coords.push_back(json::value(coords.y));
                json_player["pos"s] = j_coords;
                model::Dog::Speed speed = dog.GetSpeed();
                json::array j_speed;
                j_speed.push_back(json::value(speed.x));
                j_speed.push_back(json::value(speed.y));
                json_player["speed"s] = j_speed;
                json_player["dir"s] = dog.GetDirection();
                
                json::array loot_in_bag;
                for(auto& loot : dog.ReturnLoot()) {
                    json::object loot_info;
                    loot_info["id"] = loot->GetId();
                    loot_info["type"] = loot->GetType();
                    loot_in_bag.push_back(loot_info);
                }
                json_player["bag"] = loot_in_bag;
                json_player["score"] = player->GetValue();

                json_info[std::to_string(player->GetId())] = json_player;
                
            }
            json_response["players"s] = json_info;

            json::object json_lost_object;
            json::object json_lost_object_info;
            for (const auto& loot_object : game_.GetLootObjects()) {
                if(!loot_object->IsVisible())
                    continue;
                
                model::Dog::Coords coords = loot_object.get()->GetPosition();
                json::array j_coords;
                json_lost_object["type"] = json::value(loot_object.get()->GetType());
                j_coords.push_back(json::value(coords.x));
                j_coords.push_back(json::value(coords.y));
                json_lost_object["pos"s] = j_coords;
                json_lost_object_info[std::to_string(loot_object.get()->GetId())] = json_lost_object;
            }
            json_response["lostObjects"] = json_lost_object_info; 
        }
        catch (...) {
            json_response["code"s] = "invalidToken"s;
            json_response["message"s] = "Authorization header is required"s;
            return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }
        return MakeStringResponse(http::status::ok, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGamePlayerAction(const StringRequest& request) const {
        json::object json_response;
        if (request.method() != http::verb::post) {
            json_response["code"s] = "invalidMethod"s;
            json_response["message"s] = "Invalid method"s;
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "POST"s);
        }

        try {
            std::string request_token = { request[http::field::authorization].data(), request[http::field::authorization].size() };
            auto start = request_token.find_first_of(' ');
            if (start >= request_token.size() || !ValidToken(request_token)) {
                json_response["code"s] = "invalidToken"s;
                json_response["message"] = "Authorization header is required";
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            players::Token token(request_token.substr(start + 1));
            auto plr = player_tokens_.FindPlayerByToken(token);
            if (!plr || !plr->IsOnline()) {
                json_response["code"s] = "unknownToken"s;
                json_response["message"s] = "Player token has not been found"s;
                return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            if (request[http::field::content_type] != "application/json"s) {
                json_response["code"s] = "invalidArgument"s;
                json_response["message"s] = "Invalid content type"s;
                return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            json::value json_body = json::parse(request.body());
            std::string movement = { json_body.as_object()["move"s].as_string().data(), json_body.as_object()["move"].as_string().size() };
            plr->GetDog().SetDirection(movement);
        }
        catch (...) {
            json_response["code"s] = "invalidToken"s;
            json_response["message"] = "Authorization header is required"s;
            return MakeStringResponse(http::status::unauthorized, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }


        return MakeStringResponse(http::status::ok, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGameTick(const StringRequest& request) {
        json::object json_response;
        if (request.method() != http::verb::post) {
            json_response["code"] = "invalidMethod"s;
            json_response["message"] = "Invalid method"s;
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "POST"s);
        }

        try {
            if (request[http::field::content_type] != "application/json"s) {
                json_response["code"s] = "invalidArgument"s;
                json_response["message"] = "Failed to parse tick request JSON"s;
                return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
            }
            json::value json_body = json::parse(request.body());
            int time = json_body.as_object()["timeDelta"s].as_int64();
            game_.GenerateLoot(std::chrono::duration_cast<std::chrono::milliseconds>(time * 1.0ms));
            UpdateGameState(time);
        }
        catch (...) {
            json_response["code"s] = "invalidArgument"s;
            json_response["message"] = "Failed to parse tick request JSON"s;
            return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }

        return MakeStringResponse(http::status::ok, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::HandleApiRequestGameRecords(const StringRequest& request) {
        json::object json_response;
        int start = 0;
        int max_items = 100;

        if(!(request.method() == http::verb::get || request.method() == http::verb::head)) {
            json_response["code"s] = "ivnalidMethod"s;
            json_response["message"s] = "Only GET and HEAD methods are expected"s;
            return MakeStringResponseAllowed(http::status::method_not_allowed, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv, "POST"s);
        }

        try {
            auto begin_start = request.target().find_first_of('=') + 1;
            if(begin_start < request.target().size()) {
                auto end_start = request.target().find_first_of('&');
                std::string start_str = {request.target().substr(begin_start, end_start - begin_start).data(), request.target().substr(begin_start, end_start - begin_start).size()};
                start = std::stoi(start_str);

                auto target_substr = request.target().substr(end_start);
                auto begin_max = target_substr.find_first_of('=') + 1;
                auto end_max = target_substr.find_first_of('&');
                std::string max_str = {target_substr.substr(begin_max, end_max - begin_max).data(), target_substr.substr(begin_max, end_max - begin_max).size()};
                max_items = std::stoi(max_str);
                
                if(max_items > 100) {
                    json_response["code"s] = "invalidArgument"s;
                    json_response["message"s] = "MaxItems should be not greater than 100"s;
                    return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
                }
            }
            
        } catch (...) {
            json_response["code"s] = "invalidArgument"s;
            json_response["message"s] = "Game record request parse error"s;
            return MakeStringResponse(http::status::bad_request, serialize(json_response), request.version(), request.keep_alive(), "application/json"sv);
        }

        json::array json_info;
        {
            auto conn = conn_pool_.GetConnection();
            pqxx::read_transaction r{*conn};
            auto request_text = "SELECT name, score, play_time_ms from retired_players ORDER BY score DESC, play_time_ms, name OFFSET "s + std::to_string(start) + " LIMIT "s + std::to_string(max_items) + ";"s;
            for(auto [name, score, play_time] : r.query<std::string, int, double>(request_text)) {
                json::object player_record;
                player_record["name"s] = name;
                player_record["score"s] = score;
                player_record["playTime"s] = play_time;
                json_info.push_back(player_record);
            }
        }
      
        return MakeStringResponse(http::status::ok, serialize(json_info), request.version(), request.keep_alive(), "application/json"sv);
    }

    StringResponse RequestHandler::MakeStringError(http::status status, unsigned http_version) const {
        return MakeStringError(status, http_version, "application/json"s);
    }

    StringResponse RequestHandler::MakeStringError(http::status status, unsigned http_version, std::string_view content_type) const {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        if (status == http::status::not_found) {
            response.body() = ContentType::JSON_NOT_FOUND;
        }

        else if (status == http::status::bad_request)
            response.body() = ContentType::JSON_BAD_REQUEST;
        else
            response.body() = "Unexpected error";

        return response;
    }

    std::string RequestHandler::ConvertMapsToString() const {
        json::array target;

        for (auto map : game_.GetMaps()) {
            json::object json_map;
            json_map["id"s] = *map.GetId();
            json_map["name"s] = map.GetName();
            target.push_back(json_map);

        }
        return serialize(target);
    }

    std::string RequestHandler::FileBodyType(const std::string& body) const {
        if (body == "htm"sv || body == "html") {
            return "text/html"s;
        }
        else if (body == "css"sv)
            return "text/css"s;
        else if (body == "txt"sv)
            return "text/plain"s;
        else if (body == "js"sv)
            return "text/javascript"s;
        else if (body == "json"sv)
            return "application/json"s;
        else if (body == "xml"sv)
            return "application/xml"s;
        else if (body == "png"sv)
            return "image/png"s;
        else if (body == "jpg"sv || body == "jpe"sv || body == "jpeg"sv)
            return "image/jpeg"s;
        else if (body == "gif"sv)
            return "image/gif"s;
        else if (body == "bmp"sv)
            return "image/bmp"s;
        else if (body == "ico"sv)
            return "image/vnd.microsoft.icon"s;
        else if (body == "tiff"sv || body == "tif"sv)
            return "image/tiff"s;
        else if (body == "svg"sv || body == "svgz"sv)
            return "image/svg+xml";
        else if (body == "mp3"sv)
            return "audio/mpeg"s;
        else
            return "application/octet-stream"s;
    }

    std::string RequestHandler::GetFileType(beast::string_view body) const {
        if (body.empty()) return "";

        std::string result;

        for (int i = body.size() - 1; i >= 0; --i) {
            if (body[i] == '.')
                break;
            result += tolower(body[i]);
        }

        std::reverse(result.begin(), result.end());

        return FileBodyType(result);
    }

    bool RequestHandler::IsSubPath(fs::path path) const {
        fs::path base = fs::weakly_canonical(root_);
        path = fs::weakly_canonical(path);

        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        
        return true;
    }

    bool RequestHandler::ValidToken(const std::string& token) const {
        if (token.size() != 39) return false;
        if (token.substr(0, 7) != "Bearer ") return false;
        for (const char l : token.substr(7)) {
            if (!(std::isdigit(l) || (std::tolower(l) - 'a' >= 0 && std::tolower(l) - 'a' <= 5)))
                return false;
        }
        return true;
    }

    void RequestHandler::UpdateGameState(int time) {
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
            } else if (speed_y - std::numeric_limits<double>::epsilon() > 0) {
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
                    continue;  
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
}  // namespace http_handler

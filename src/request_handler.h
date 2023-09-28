#pragma once

#include "db_connection.h"
#include "http_server.h"
#include "model.h"
#include "player.h"
#include "util/tagged.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>


#include <filesystem>
#include <functional>
#include <memory>
#include <variant>
#include <iostream>
#include <utility>

namespace beast = boost::beast;
namespace fs = std::filesystem;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio; 
namespace sys = boost::system;

using namespace std::literals;
using tcp = net::ip::tcp;

namespace http_handler {
    class Ticker : public std::enable_shared_from_this<Ticker> {
    public:
        using Strand = net::strand<net::io_context::executor_type>;
        using Handler = std::function<void(std::chrono::milliseconds delta)>;

        Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
            : strand_{ strand }
            , period_{ period }
            , handler_{ std::move(handler) } {
        }

        void Start() {
            net::dispatch(strand_, [self = shared_from_this()] {
                self->last_tick_ = Clock::now();
                self->ScheduleTick();
                });
        }

    private:
        void ScheduleTick() {
            assert(strand_.running_in_this_thread());
            timer_.expires_after(period_);
            timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
                self->OnTick(ec);
                });
        }

        void OnTick(sys::error_code ec) {
            using namespace std::chrono;
            assert(strand_.running_in_this_thread());

            if (!ec) {
                auto this_tick = Clock::now();
                auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
                last_tick_ = this_tick;
                try {
                    handler_(delta);
                }
                catch (...) {
                }
                ScheduleTick();
            }
        }

        using Clock = std::chrono::steady_clock;

        Strand strand_;
        std::chrono::milliseconds period_;
        net::steady_timer timer_{strand_};
        Handler handler_;
        std::chrono::steady_clock::time_point last_tick_;
    };

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;
    using FileResponse = http::response<http::file_body>;
    using EmptyResponse = http::response<http::string_body>;

    class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
    public:
        using Strand = net::strand<net::io_context::executor_type>;

        RequestHandler(fs::path root, Strand api_strand, model::Game& game, players::Players& players, players::PlayerTokens& tokens, 
                        int tick_period, conn_pool::ConnectionPool& conn_pool, int save_period, std::string save_path)
            : root_{ std::move(root) }
            , api_strand_{ api_strand }
            , game_{ game }
            , players_{ players }
            , player_tokens_{ tokens }
            , tick_period_{ tick_period } 
            , conn_pool_{conn_pool}
            , save_period_{save_period}
            , save_path_{save_path}
        {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        std::pair<int, std::string> operator()(tcp::endpoint ep, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            auto version = req.version();
            auto keep_alive = req.keep_alive();
            const int minimum_get_request_size = 5;

            if (req.target() == "/favicon.ico"sv) return GetLogInfo();

            try {
                if (req.target().substr(0, minimum_get_request_size) == "/api/"sv) {
                    auto handle = [self = shared_from_this(), send,
                        req = std::forward<decltype(req)>(req), version, keep_alive] {
                        try {
                            // Этот assert не выстрелит, так как лямбда-функция будет выполняться внутри strand
                            assert(self->api_strand_.running_in_this_thread());
                            send(self->HandleApiRequest(req));
                            return self->GetLogInfo();
                        }
                        catch (...) {
                            send(self->ReportServerError(version, keep_alive));
                        }
                    };
                    net::dispatch(api_strand_, handle);
                    return GetLogInfo();
                }
                std::visit(
                    [&send](auto&& result) {
                        send(std::forward<decltype(result)>(result));
                    },
                    HandleFileRequest(req));
                return GetLogInfo();
            }
            catch (...) {
                send(ReportServerError(version, keep_alive));
            }
            return GetLogInfo();
        }

        std::pair<int, std::string> GetLogInfo() {
            return { status_, content_type_ };
        }

    private:
        using FileRequestResult = std::variant<EmptyResponse, StringResponse, FileResponse>;

        FileRequestResult HandleFileRequest(const StringRequest& req);
        StringResponse HandleApiRequest(const StringRequest& request);
        StringResponse MakeStringError(http::status, unsigned) const;
        StringResponse MakeStringError(http::status, unsigned, std::string_view) const;
        StringResponse ReportServerError(unsigned version, bool keep_alive);
        StringResponse MakeStringResponse(http::status, std::string_view, unsigned, bool, std::string_view) const;
        StringResponse MakeStringResponseAllowed(http::status, std::string_view, unsigned, bool, std::string_view, std::string) const;
        StringResponse HandleApiRequestJoinGame(const StringRequest& request);
        StringResponse HandleApiRequestGetMaps(const StringRequest& reqeust) const;
        StringResponse HandleApiRequestGetMap(const StringRequest& request) const;
        StringResponse HandleApiRequestGetPlayers(const StringRequest& request) const;
        StringResponse HandleApiRequestGameState(const StringRequest& request) const;
        StringResponse HandleApiRequestGamePlayerAction(const StringRequest& request) const;
        StringResponse HandleApiRequestGameTick(const StringRequest& request);
        StringResponse HandleApiRequestGameRecords(const StringRequest& request);
        

        std::string ConvertMapsToString() const;
        std::string FileBodyType(const std::string& body) const;
        std::string GetFileType(beast::string_view body) const;
        bool IsSubPath(fs::path path) const;
        bool ValidToken(const std::string& token) const;
        void UpdateGameState(int time);

        fs::path root_;
        Strand api_strand_;
        model::Game& game_;
        players::Players& players_;
        players::PlayerTokens& player_tokens_;
        int tick_period_;
        conn_pool::ConnectionPool& conn_pool_;
        int save_period_ = 0;
        int prev_saving_ = 100;
        std::string save_path_;
        int status_ = 200;
        std::string content_type_ = "application/json"s;
        /* прочие данные */

        struct ContentType {
            ContentType() = delete;
            constexpr static std::string_view JSON_BAD_REQUEST = R"({
  "code": "badRequest",
  "message" : "Bad request"
})";
            constexpr static std::string_view JSON_NOT_FOUND = R"({
  "code": "mapNotFound",
  "message" : "Map not found"
})";


        };
    };
} // namespace http_hadler
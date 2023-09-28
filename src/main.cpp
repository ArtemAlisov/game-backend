#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/program_options.hpp>
#include <boost/asio/io_context.hpp>

#include "sdk.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "application.h"
#include "db_connection.h"
#include "json_loader.h"
#include "request_handler.h"
#include "log_response.h"
#include "player.h"
#include "serialization.h"

#include <boost/date_time.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

using namespace std::literals;
namespace fs = std::filesystem;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

    struct Args {
        std::string file;
        std::string dir;
        std::string state_file;
        std::chrono::milliseconds tick_period = 0ms;
        std::chrono::milliseconds save_period = 0ms;
        bool randomize = false;
    };

    [[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
        namespace po = boost::program_options;

        po::options_description desc{"Allowed options"s};

        std::string tick_period;
        std::string save_period;
        Args args;
        desc.add_options()
            ("help,h", "produce help message")
            ("tick-period,t", po::value(&tick_period)->value_name("millisec"), "set tick period")
            ("config-file,c", po::value(&args.file)->value_name("file"), "set config file path")
            ("www-root,w", po::value(&args.dir)->value_name("dir"), "set static files root")
            ("randomize-spawn-points,r", "spawn dogs at random positions")
            ("state-file,s", po::value(&args.state_file)->value_name("file"), "set state file path")
            ("save-state-period,p", po::value(&save_period)->value_name("millisec"), "set state save period");


        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.contains("help"s)) {
            std::cout << desc;
            return std::nullopt;
        }

        if (vm.contains("randomize-spawn-points"s)) {
            args.randomize = true;
        }

        if (vm.contains("tick-period"s)) {
            args.tick_period = static_cast<std::chrono::milliseconds>(stoi(tick_period));
        }

        if (!vm.contains("config-file"s)) {
            throw std::runtime_error("Config file path have not been specified"s);
        }
        if (!vm.contains("www-root"s)) {
            throw std::runtime_error("Static files root is not specified"s);
        }

        if (vm.contains("save-period"s)) {
            args.save_period = static_cast<std::chrono::milliseconds>(stoi(save_period));
        }

        return args;
    }

    template <typename Fn>
    void RunWorkers(unsigned threads, const Fn& fn) {
        threads = std::max(1u, threads);
        std::vector<std::jthread> workers;
        workers.reserve(threads - 1);
        while (--threads) {
            workers.emplace_back(fn);
        }
        fn();
    }

}  // namespace

int main(int argc, const char* argv[]) {
    
    Args game_args;
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            game_args = *args;
        }
        else {
            return 0;
        }
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    log_response::LoggingRequestHandler<http_handler::RequestHandler>::InitBoostLogFilter();

    try {
        const unsigned num_threads = std::thread::hardware_concurrency();

        if(game_args.state_file.empty())
            game_args.save_period = 0ms;

        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) {
            throw std::runtime_error("GAME_DB_URL is not specified");
        }
        conn_pool::ConnectionPool conn_pool{num_threads, [db_url] {
            auto conn = std::make_shared<pqxx::connection>(db_url);
            return conn;
        }};

        model::Game game = json_loader::LoadGame(game_args.file);
        if(game_args.randomize)
            game.SetRandomMode();
        players::Players players;
        players::PlayerTokens player_tokens;

        if(!game_args.state_file.empty()) 
            serializer::DeserializeGame(game_args.state_file, game, players, player_tokens);

        net::io_context ioc(num_threads);

        fs::path static_files_root = game_args.dir;

        auto api_strand = net::make_strand(ioc);

        net::signal_set signals(ioc, SIGINT, SIGTERM); 
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        auto handler = std::make_shared<http_handler::RequestHandler>(
            static_files_root, api_strand, game, players, player_tokens, game_args.tick_period.count(), conn_pool, game_args.save_period.count(), game_args.state_file);
        
        application::Application app{game, players, player_tokens, conn_pool, game_args.save_period.count(), game_args.state_file};

        log_response::LoggingRequestHandler logging_handler{
            [handler](auto&& endpoint, auto&& req, auto&& send) {
                return (*handler)(std::forward<decltype(endpoint)>(endpoint),
                    std::forward<decltype(req)>(req),
                    std::forward<decltype(send)>(send));
            }};
          
        const auto address = net::ip::make_address("0.0.0.0"); 
        constexpr net::ip::port_type port = 8080;  

        http_server::ServeHttp(ioc, { address, port }, logging_handler);

        if(game_args.tick_period != 0ms) {
            std::chrono::milliseconds delta = game_args.tick_period;
            auto ticker = std::make_shared<http_handler::Ticker>(api_strand, delta,
            [&app](std::chrono::milliseconds delta) { app.Tick(delta); }
            );
            ticker->Start();
        }

        log_response::LoggingRequestHandler<http_handler::RequestHandler>::LogStart(port, address.to_string());
        
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        if(!game_args.state_file.empty()) 
            serializer::SerializeGame(game_args.state_file, game, players, player_tokens);
    } catch (const std::exception& ex) {
        log_response::LoggingRequestHandler<http_handler::RequestHandler>::LogEnd(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
    
    log_response::LoggingRequestHandler<http_handler::RequestHandler>::LogEnd(0, "");

}

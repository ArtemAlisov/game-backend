#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <thread>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>

#include "request_handler.h"

namespace log_response {

	namespace logging = boost::log;
	namespace keywords = boost::log::keywords;
	namespace json = boost::json;
	namespace beast = boost::beast;
	namespace http = beast::http;

	using namespace std::literals;

	BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
	BOOST_LOG_ATTRIBUTE_KEYWORD(msg, "Msg", std::string)
	BOOST_LOG_ATTRIBUTE_KEYWORD(data, "Data", json::value)


	template<class SomeRequestHandler>
	class LoggingRequestHandler {
		static void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
			auto ts = rec[timestamp];
			strm << "{\"timestamp\":\""sv << to_iso_extended_string(*ts) << "\""sv;
			strm << ",\"data\":"sv << rec[data];
			strm << ",\"message\":\""sv << rec[msg] << "\"}"sv << std::endl;
		}

		

		template <typename Body, typename Allocator>
		static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& r) {
			std::string request_msg = "request received"s;
			json::object request_data;
			auto ip = r.base()[http::field::host].substr(0, r.base()[http::field::host].find_first_of(':'));
			request_data["ip"] = std::string{ ip.data(), ip.size() };
			request_data["URI"] = std::string{r.target().data(), r.target().size()};
			request_data["method"] = MethodToString(r.method());
			BOOST_LOG_TRIVIAL(info) << logging::add_value(data, request_data)
				<< logging::add_value(msg, request_msg);
		}

		template <typename Timer>
		static void LogResponse(const Timer& response_time, int code, const std::string& content_type) {
			std::string response_msg = "response sent"s;
			json::object response_data;
			response_data["response_time"] = static_cast<int>(response_time);
			response_data["code"] = code;
			response_data["content_type"] = content_type;
			BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data)
				<< logging::add_value(msg, response_msg);
		}

		static std::string MethodToString(http::verb method) {
			if (method == http::verb::get)
				return "GET";
			else if (method == http::verb::post)
				return "POST";
			else if (method == http::verb::put)
				return "PUT";
			else if (method == http::verb::delete_)
				return "DELETE";
			else if (method == http::verb::head)
				return "HEAD";
			else
				return "UNKNOWN";
		}

	public:
		explicit LoggingRequestHandler(SomeRequestHandler handler)
			: decorated_(handler)
		{
		}

		static void LogStart(int port, const std::string& address) {
			std::string log_msg = "server started"s;
			json::object log_data;
			log_data["port"] = port;
			log_data["address"] = address;
			BOOST_LOG_TRIVIAL(info) << logging::add_value(data, log_data)
				<< logging::add_value(msg, log_msg);
		}

		static void LogEnd(int code, const std::string& exception_log) {
			std::string log_msg = "server exited"s;
			json::object log_data;
			log_data["code"] = code;
			if(!exception_log.empty())
				log_data["exception"] = exception_log;
			BOOST_LOG_TRIVIAL(info) << logging::add_value(data, log_data)
				<< logging::add_value(msg, log_msg);
		}

		static void LogError(int code, const std::string& exception_log, const std::string& where_log) {
			std::string log_msg = "error"s;
			json::object log_data;
			log_data["code"] = code;
			log_data["exception"] = exception_log;
			log_data["where"] = where_log;
			BOOST_LOG_TRIVIAL(info) << logging::add_value(data, log_data)
				<< logging::add_value(msg, log_msg);
		}

		static void InitBoostLogFilter() {
			boost::log::add_common_attributes();

			boost::log::add_console_log(
				std::cout,
				boost::log::keywords::format = &MyFormatter,
				boost::log::keywords::auto_flush = true

			);
		}

		static void LogStartServer(const std::string& address, const std::string& port) {
			json::value custom_data;
		}



		template <typename Body, typename Allocator, typename Send>
		void operator()(tcp::endpoint ep, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
			if (req.target() == "/favicon.ico") 
				return;
			LogRequest(req);
			std::chrono::system_clock::time_point start_ts = std::chrono::system_clock::now(); 
			std::pair<int, std::string> data = decorated_(ep, std::move(req), std::move(send));
			std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();
			auto response_duration = (end_ts - start_ts).count();

			LogResponse(response_duration, data.first, data.second);

		}

	private:
		SomeRequestHandler decorated_;
	};

} // namespace log_response
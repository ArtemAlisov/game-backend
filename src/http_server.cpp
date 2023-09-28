#include "log_response.h"
#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

using namespace std::literals;

namespace http_server {

    void ReportError(beast::error_code ec, std::string_view what) {
        log_response::LoggingRequestHandler<http_handler::RequestHandler>::LogError(ec.value(), ec.message().data(), what.data());
    }

    void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        if (ec) {
            return ReportError(ec, "write"sv);
        }

        if (close) {
            return Close();
        }

        Read();
    }

    void SessionBase::Run() {
        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }


    void SessionBase::Read() {
        using namespace std::literals;
        request_ = {};
        stream_.expires_after(30s);
        http::async_read(stream_, buffer_, request_,
            beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
    }


    void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        using namespace std::literals;
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return ReportError(ec, "read"sv);
        }
        HandleRequest(std::move(request_));
    }


    void SessionBase::Close() {
        stream_.socket().shutdown(tcp::socket::shutdown_send);
    }
    
}  // namespace http_server
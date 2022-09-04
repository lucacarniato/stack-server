#include <chrono>
#include <stack>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/mutex.hpp>

#include "TCPConnection.hpp"
#include "Utils.hpp"

using namespace stack_server;

void TCPConnection::read_request_header()
{
    // read the header
    boost::system::error_code error_code;
    boost::asio::read(socket_, boost::asio::buffer(connection_buffer_.data(), 1), error_code);

    // read the request type
    const auto is_push_request = (connection_buffer_[0] & 128) == 0;
    const auto is_pop_request = (connection_buffer_[0] & 128) == 128;

    if (is_push_request)
    {
        payload_size_ = compute_int_from_byte(connection_buffer_[0]);
        request_type_ = RequestType::Push;
    }
    else if (is_pop_request)
    {
        request_type_ = RequestType::Pop;
    }
}

void TCPConnection::handle_request()
{
    if (request_type_ == RequestType::Push)
    {
        async_read_payload_and_push_message();
    }
    if (request_type_ == RequestType::Pop)
    {
        async_write_payload_and_pop_message();
    }
}

void TCPConnection::write_busy_state_response()
{
    boost::system::error_code error_code;
    std::vector<unsigned char> connection_buffer(1, 0xFF);
    boost::asio::write(socket_, boost::asio::buffer(connection_buffer, 1), error_code);
    socket_.close();
}

double TCPConnection::get_seconds_from_start_connection() const
{
    const auto current_time = std::chrono::steady_clock::now();
    const auto seconds_from_start = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_connection_time_).count() / 1000.0;
    return seconds_from_start;
}

void TCPConnection::set_start_connection_time()
{
    start_connection_time_ = std::chrono::steady_clock::now();
}

bool TCPConnection::is_closed()
{
    // server closed the socket
    if (socket_.is_open() == false)
    {
        return true;
    }

    // client-side by reading
    std::vector<unsigned char> connection_buffer(1, 0);
    boost::system::error_code error_code;
    socket_.non_blocking(true);
    read(socket_, boost::asio::buffer(connection_buffer.data(), 1), error_code);

    // if error, the client closed the connection and the read generate one of these errors
    if (error_code.value() == boost::asio::error::would_block ||
        error_code.value() == boost::asio::error::shut_down ||
        error_code.value() == boost::asio::error::timed_out)
    {
        socket_.non_blocking(false);
        return true;
    }
    // else connection is still opened
    socket_.non_blocking(false);
    return false;
}

void TCPConnection::read_payload_and_push_message()
{
    boost::system::error_code error_code;
    read(socket_, boost::asio::buffer(connection_buffer_.data() + 1, payload_size_), error_code);
    message_stack_.emplace(connection_buffer_.begin(), connection_buffer_.begin() + payload_size_ + 1);
    write_valid_response();
}

void TCPConnection::write_payload_and_pop_message()
{
    boost::system::error_code error_code;
    write(socket_, boost::asio::buffer(message_stack_.top(), message_stack_.top().size()), error_code);
    message_stack_.pop();
    write_valid_response();
}

TCPConnection::TCPConnection(boost::asio::io_service& io_service,
                             std::stack<std::vector<unsigned char>>& message_stack,
                             std::vector<connection_pointer>& long_pop_polled_connections,
                             std::vector<connection_pointer>& long_push_polled_connections)
    : io_service_{io_service},
      strand_(io_service_),
      socket_(io_service_),
      message_stack_(message_stack),
      long_pop_polled_connections_(long_pop_polled_connections),
      long_push_polled_connections_(long_push_polled_connections)
{
    connection_buffer_.resize(buffer_size);
}

void TCPConnection::async_read_payload_and_push_message()
{
    boost::system::error_code error_code;

    async_read(socket_, boost::asio::buffer(connection_buffer_.data() + 1, payload_size_),
               bind_executor(strand_, boost::bind(&TCPConnection::handle_push_request, shared_from_this(), error_code)));
}

void TCPConnection::handle_push_request(const boost::system::error_code& error_code)
{
    if (error_code)
    {
        return;
    }
    message_stack_.emplace(connection_buffer_.begin(), connection_buffer_.begin() + payload_size_ + 1);

    // now at least one message is in the message stack, so we can process the oldest long_pop_polled_connections_
    if (long_pop_polled_connections_.empty() == false)
    {
        long_pop_polled_connections_.front()->write_payload_and_pop_message();
        long_pop_polled_connections_.erase(long_pop_polled_connections_.begin());
    }

    write_valid_response();
}

void TCPConnection::async_write_payload_and_pop_message()
{
    boost::system::error_code error_code;
    async_write(socket_, boost::asio::buffer(message_stack_.top().data(), message_stack_.top().size()),
                bind_executor(strand_, boost::bind(&TCPConnection::handle_pop_request, shared_from_this(),
                                                   error_code)));
}

void TCPConnection::handle_pop_request(const boost::system::error_code& error_code)
{
    if (error_code)
    {
        return;
    }
    message_stack_.pop();

    // now at least one message is removed from the message stack, so we can process the oldest long_push_polled_connections_
    if (long_pop_polled_connections_.empty() == false)
    {
        long_push_polled_connections_.front()->read_payload_and_push_message();
        long_push_polled_connections_.erase(long_push_polled_connections_.begin());
    }
    write_valid_response();
}

void TCPConnection::write_valid_response()
{
    boost::system::error_code error_code;
    std::vector<unsigned char> connection_buffer(1, 0);
    boost::asio::write(socket_, boost::asio::buffer(connection_buffer, 1), error_code);
    socket_.close();
}

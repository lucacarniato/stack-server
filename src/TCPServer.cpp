#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>

#include "TCPServer.hpp"
#include "TCPConnection.hpp"
#include "RequestType.hpp"


using namespace stack_server;

TCPServer::TCPServer(int port,
                     int max_message_stack_size,
                     int max_num_connections,
                     double expired_connection_time)
    : max_message_stack_size_{max_message_stack_size},
      max_num_connections_{max_num_connections},
      expired_connection_seconds_{expired_connection_time},
      acceptor_(io_service_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
    const auto new_connection = TCPConnection::create(io_service_, message_stack_, long_pop_polled_connections_, long_push_polled_connections_);
    boost::system::error_code ec;
    acceptor_.async_accept(new_connection->get_socket(), boost::bind(&TCPServer::handle_connection, this, new_connection, ec));
}

void TCPServer::handle_connection(TCPConnection::connection_pointer accepted_connection, const boost::system::error_code& error_code)
{
    // Return if an error 
    if (error_code)
    {
        return;
    }

    // Connection established, start connection
    accepted_connection->set_start_connection_time();

    // Remove all closed connections
    remove_closed_connections(connections_);
    remove_closed_connections(long_push_polled_connections_);
    remove_closed_connections(long_pop_polled_connections_);

    // When a new incoming connection is coming, kill the oldest connection when defined conditions are met
    if (connections_.size() >= max_num_connections_ && connections_.front()->get_seconds_from_start_connection() > expired_connection_seconds_)
    {
        connections_.erase(connections_.begin());
    }

    // Read the header
    accepted_connection->read_request_header();

    // Process connection
    if (accepted_connection->get_request_type() == RequestType::Push && message_stack_.size() >= max_message_stack_size_)
    {
        long_push_polled_connections_.emplace_back(accepted_connection);
    }
    else if (accepted_connection->get_request_type() == RequestType::Pop && message_stack_.empty())
    {
        long_pop_polled_connections_.emplace_back(accepted_connection);
    }
    else if (connections_.size() >= max_num_connections_ && connections_.front()->get_seconds_from_start_connection() <= expired_connection_seconds_)
    {
        accepted_connection->write_busy_state_response();
    }
    else if (connections_.size() < max_num_connections_)
    {
        connections_.emplace_back(accepted_connection);
        accepted_connection->handle_request();
    }

    // Accept a new connection
    const auto new_connection = TCPConnection::create(io_service_, message_stack_, long_pop_polled_connections_, long_push_polled_connections_);
    boost::system::error_code ec;
    acceptor_.async_accept(new_connection->get_socket(), boost::bind(&TCPServer::handle_connection, this, new_connection, ec));
}

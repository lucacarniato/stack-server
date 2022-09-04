#pragma once

#include <stack>
#include <vector>

#include <boost/asio.hpp>

#include "TCPConnection.hpp"

namespace stack_server
{
    /// \brief A class representing a TCP server accepting and handling connections
    class TCPServer
    {
    public:

        /// \brief Constructor for TCPServer
        /// \param[in] port The port number
        /// \param[in] max_message_stack_size The maximum amount of messages in the stack
        /// \param[in] max_num_connections The maximum number of opened connections (excluding long polled ones)
        /// \param[in] expired_connection_time The amount of seconds for a connection to expire on a new incoming connection
        TCPServer(int port,
                  int max_message_stack_size,
                  int max_num_connections,
                  double expired_connection_time);

        ///\brief Start the server
        void start()
        {
            io_service_.run();
        }

    private:
        /// \brief Remove all closed connections from a container
        /// \param[in] connections The connections container
        template <typename T>
        static void remove_closed_connections(T& connections)
        {
            auto it = connections.begin();
            while (it != connections.end())
            {
                if ((*it)->is_closed())
                {
                    it = connections.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        /// \brief Completion handler for an accepted connection
        /// \param[in] accepted_connection The accepted connection
        /// \param[in] error_code The error code after accepting a new connection
        void handle_connection(TCPConnection::connection_pointer accepted_connection, const boost::system::error_code& error_code);

        boost::asio::io_service io_service_;      ///< The service executing queued completion handlers
        boost::asio::ip::tcp::acceptor acceptor_; ///< Accepting new socket connections.

        std::stack<std::vector<unsigned char>> message_stack_;             ///< The stack of received messages
        std::vector<TCPConnection::connection_pointer> long_pop_polled_connections_;  ///< The long polled pop connections
        std::vector<TCPConnection::connection_pointer> long_push_polled_connections_; ///< The long polled push connections
        std::vector<TCPConnection::connection_pointer> connections_;                  ///< The current connections

        int max_message_stack_size_;              ///< The maximum size of \ref message_stack_
        int max_num_connections_;                 ///< The maximum size of \ref connections_
        double expired_connection_seconds_{10.0}; ///< The amount of seconds for a connection to expire on a new incoming connection if size of \ref connections_ is equal to \ref max_num_connections_ 
    };

} // namespace stack_server

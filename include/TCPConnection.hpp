#pragma once

#include <chrono>
#include <queue>
#include <stack>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "RequestType.hpp"

namespace stack_server
{
    /// \brief A class representing a TCP connection
    class TCPConnection : public boost::enable_shared_from_this<TCPConnection>
    {
    public:
        typedef boost::shared_ptr<TCPConnection> connection_pointer;

        /// \brief Factory method for creating a new TCPConnection
        /// \param[in] io_service The service executing queued completion handlers
        /// \param[in] message_stack A reference to the message stack to update
        /// \param[in] long_pop_polled_connections A reference to long_pop_polled_connections container
        /// \param[in] long_push_polled_connections A reference to long_push_polled_connections container
        static connection_pointer create(boost::asio::io_service& io_service,
                                         std::stack<std::vector<unsigned char>>& message_stack,
                                         std::vector<connection_pointer>& long_pop_polled_connections,
                                         std::vector<connection_pointer>& long_push_polled_connections)
        {
            return boost::shared_ptr<TCPConnection>(new TCPConnection(io_service, message_stack, long_pop_polled_connections, long_push_polled_connections));
        }

        /// \brief Gets the connection socket
        /// @return The connection socket associated with this connection
        boost::asio::ip::tcp::socket& get_socket()
        {
            return socket_;
        }

        /// \brief The request type
        /// @return Gets the request type associated with this connection
        const RequestType& get_request_type() const
        {
            return request_type_;
        }

        /// \brief Reads the request header and determines the request type and payload size
        void read_request_header();

        /// \brief Handle the request asynchronously
        void handle_request();

        /// \brief Writes a busy byte response and close the socket
        void write_busy_state_response();

        /// \brief Gets the amount of seconds since the connection started
        /// \return The number of seconds  since the connection started (with milliseconds accuracy)
        double get_seconds_from_start_connection() const;

        /// \brief Sets the start connection time
        void set_start_connection_time();

        /// \brief Determines if a connection is closed (server-side or client-side by attempting to read)
        /// \return If connection is closed
        bool is_closed();

    private:
        /// \brief Constructor
        /// \param[in] io_service The service executing queued completion handlers
        /// \param[in] message_stack A reference to the message stack to update
        /// \param[in] long_pop_polled_connections A reference to long_pop_polled_connections container
        /// \param[in] long_push_polled_connections A reference to long_push_polled_connections container
        TCPConnection(boost::asio::io_service& io_service,
                      std::stack<std::vector<unsigned char>>& message_stack,
                      std::vector<connection_pointer>& long_pop_polled_connections,
                      std::vector<connection_pointer>& long_push_polled_connections);

        /// \brief Asynchronous read of the payload and pushing the message in \ref message_stack_
        void async_read_payload_and_push_message();

        /// \brief Completion handler for \ref async_read_payload_and_push_message
        void handle_push_request(const boost::system::error_code& error_code);

        /// \brief Asynchronous writing of the message to the client and popping the message from \ref message_stack_
        void async_write_payload_and_pop_message();

        /// \brief Completion handler for \ref async_write_payload_and_pop_message
        void handle_pop_request(const boost::system::error_code& error_code);

        /// \brief Synchronous reading of the request payload and pushing the message in \ref message_stack_
        void read_payload_and_push_message();

        /// \brief Synchronous writing the top of \ref message_stack_ to the client and popping it from \ref message_stack_
        void write_payload_and_pop_message();

        /// \brief Writes a valid response and close the socket
        void write_valid_response();

        boost::asio::io_service& io_service_;                           ///< The service executing queued completion handlers
        boost::asio::io_service::strand strand_;                        ///< Strand to ensure only one thread operates on the message stack
        boost::asio::ip::tcp::socket socket_;                           ///< Connection socket
        std::stack<std::vector<unsigned char>>& message_stack_;         ///< A reference to the message stack container
        std::vector<connection_pointer>& long_pop_polled_connections_;  ///< A reference to the long_pop_polled_connections container
        std::vector<connection_pointer>& long_push_polled_connections_; ///< A reference to long pushed connections container

        std::chrono::steady_clock::time_point start_connection_time_; ///< The start connection time
        std::vector<unsigned char> connection_buffer_;                ///< The buffer for storing the read message
        RequestType request_type_{RequestType::None};                 ///< The request type for this connection
        int payload_size_{0};                                         ///< The payload size of the request
        static inline int constexpr buffer_size = 128;                ///< The maximum buffer size
    };
} // namespace stack_server

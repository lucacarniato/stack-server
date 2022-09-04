#include <iostream>

#include <boost/asio.hpp>

#include "TCPServer.hpp"

using namespace stack_server;

int main()
{
    try
    {
        TCPServer server(8080, 100, 100, 10);
        server.start();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}


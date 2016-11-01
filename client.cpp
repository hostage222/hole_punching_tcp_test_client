#include "client.h"

#include <string>
#include <array>
#include <vector>
#include <memory>
#include <functional>
#include <boost/asio.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
using boost_error = boost::system::error_code;

client::client(string name, string server_ip, uint16_t server_port,
               string friend_name) :
    server_socket{service},
    name{move(name)},
    server_endpoint{ip::address::from_string(server_ip), server_port},
    friend_name{move(friend_name)},
    friend_server_socket{service},
    acceptor{service}
{
}

client::ptr client::create(string name, string server_ip, uint16_t server_port,
                           string friend_name)
{
    client *p = new client{move(name), move(server_ip), server_port,
                           move(friend_name)};
    return ptr{p};
}

void client::run()
{
    server_socket.async_connect(server_endpoint,
        [this](boost_error ec)
        {
            if (!ec)
            {
                open_connection();
            }
            else
            {
                cout << "connection error: " << ec << endl;
                close_all();
            }
        }
    );
    service.run();
}

void client::write(string text)
{

}

void client::open_connection()
{
    fill_private_endpoint();
    if (!start_acceptor())
    {
        return;
    }
    send_connect();
}

void client::send_connect()
{
    server_buf = "connect " + name + " " + to_string(private_endpoint) + "\r\n";
    async_write(server_socket, buffer(server_buf),
        [this](boost_error ec, size_t)
        {
            if (!ec)
            {
                start_read(&client::handle_connect);
            }
            else
            {
                cout << "connect error: " << ec << endl;
                close_all();
            }
        }
    );
}

void client::handle_connect(string answer)
{
    if (answer == "confirm_connection")
    {
        if (!friend_name.empty())
        {
            send_get_list();
        }
    }
    else
    {
        cout << "inalid answer for \"connect\": " << answer;
    }
}

void client::send_get_list()
{
    server_buf = "get_list\r\n";
    async_write(server_socket, buffer(server_buf),
        [this](boost_error ec, size_t)
        {
            if (!ec)
            {
                start_read(handle_get_list);
            }
            else
            {
                cout << "get_list error: " << ec << endl;
                close_all();
            }
        }
    );
}

void client::handle_get_list(string answer)
{
    cout << "get_list handler: " << answer << endl;
    close_all();
}

bool client::start_acceptor()
{
    boost_error ec;
    acceptor.open(private_endpoint.protocol(), ec);
    if (ec)
    {
        cout << "acceptor open error: " << ec << endl;
        close_all();
        return false;
    }
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                        ec);
    if (ec)
    {
        cout << "acceptor so_reuseaddress error: " << ec << endl;
        close_all();
        return false;
    }
    acceptor.bind(private_endpoint, ec);
    if (ec)
    {
        cout << "acceptor bind error: " << ec << endl;
        close_all();
        return false;
    }
    acceptor.listen();

    acceptor.async_accept(friend_server_socket,
        [this](boost_error ec)
        {
            if (!ec)
            {
                cout << "communication accepted" << endl;
                //continue: communication
                close_all();
            }
            else
            {
                cout << "accept friend error: " << ec << endl;
                close_all();
            }
        }
    );

    return true;
}

void client::fill_private_endpoint()
{
    tcp::resolver res{service};
    string host = ip::host_name();

    auto it = find_if(res.resolve({host, ""}), {},
        [](const auto &re)
        { return re.endpoint().address().is_v4() &&
                !re.endpoint().address().is_loopback(); }
    );
    if (it != tcp::resolver::iterator{})
    {
        private_endpoint = tcp::endpoint{it->endpoint().address(),
                                         server_socket.local_endpoint().port()};
    }
    else
    {
        private_endpoint = tcp::endpoint{tcp::v4(),
                                         server_socket.local_endpoint().port()};
    }
}

void client::start_read(void(client::*handler)(string))
{
    using namespace std::placeholders;
    async_read(server_socket, buffer(server_read_buf),
               bind(&client::read_complete,
                    shared_from_this(), _1, _2),
               bind(&client::read,
                    shared_from_this(),
                    [self = shared_from_this(), handler](std::string answer)
                    { ((*self).*handler)(move(answer)); },
                    _1, _2));
}

size_t client::read_complete(boost_error ec, size_t bytes)
{
    if (ec)
    {
        close_all();
        return 0;
    }

    auto end = server_read_buf.begin() + bytes;
    auto start_it = find_if(server_read_buf.begin(), end,
                            [](char c){ return !isspace(c); });
    if (start_it == end)
    {
        return 1;
    }

    auto finish_it = find_if(next(start_it), end,
                             [](char c){ return isspace(c) && c != ' '; });
    if (finish_it == end)
    {
        return 1;
    }

    return 0;
}

void client::read(function<void (string)> handler,
                  boost_error ec, size_t bytes)
{
    if (ec)
    {
        close_all();
        return;
    }

    auto end = server_read_buf.begin() + bytes;
    auto start_it = find_if(server_read_buf.begin(), end,
                            [](char c){ return !isspace(c); });
    auto finish_it = find_if(next(start_it), end,
                             [](char c){ return isspace(c) && c != ' '; });

    string request{start_it, finish_it};
    handler(move(request));
}

void client::close_all()
{
    server_socket.close();
    acceptor.close();
    friend_server_socket.close();
}

string client::to_string(tcp::endpoint endpoint)
{
    return endpoint.address().to_string() + " " + ::to_string(endpoint.port());
}

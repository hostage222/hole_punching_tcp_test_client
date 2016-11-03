#include "client.h"

#include <string>
#include <array>
#include <vector>
#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include <boost/date_time.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
using boost_error = boost::system::error_code;

static const auto FRIEND_REPEAT_PERIOD = boost::posix_time::seconds(1);

client::client(string name, string server_ip, uint16_t server_port,
               string friend_name) :
    server_socket{service},
    name{move(name)},
    server_endpoint{ip::address::from_string(server_ip), server_port},
    friend_name{move(friend_name)},
    friend_repeat_timer{service},
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
                cout << "connection error: " << ec.message() << endl;
                close_all();
            }
        }
    );
    service.run();
}

void client::write(const string &text)
{
    service.post(
        [self = shared_from_this(), text]
        {
            if (self->state == state_type::communicate_friend)
            {
                bool write_in_progress = !self->output_messages.empty();
                string mes = "message " + self->name + " " + text + "\r\n";
                self->output_messages.push(move(mes));
                if (self->output_messages.size() > MAX_SAVED_OUTPUT_MESSAGES)
                {
                    cout << "<LOSTED MESSAGE>: " <<
                            self->output_messages.front() << endl;
                    self->output_messages.pop();
                }
                if (!write_in_progress)
                {
                    self->do_friend_write();
                }
            }
            else
            {
                cout << "What are you doing man? I'm trying to connect." <<
                        endl;
            }
        }
    );
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
                cout << "connect error: " << ec.message() << endl;
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
        close_all();
    }
}

void client::send_get_list()
{
    if (state != state_type::wait_friend)
    {
        return;
    }

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
                cout << "get_list error: " << ec.message() << endl;
                close_all();
            }
        }
    );
}

void client::handle_get_list(string answer)
{
    string title = get_token(&answer);
    if (title != "list")
    {
        cout << "get_list invalid answer" << endl;
        close_all();
        return;
    }

    if (state != state_type::wait_friend)
    {
        return;
    }

    string cl;
    while (!(cl = get_token(&answer)).empty())
    {
        if (cl == friend_name)
        {
            send_get_info();
            return;
        }
    }

    friend_repeat_timer.expires_from_now(FRIEND_REPEAT_PERIOD);
    friend_repeat_timer.async_wait(
        [this](boost_error ec)
        {
            if (!ec)
            {
                send_get_list();
            }
            else
            {
                cout << "repeat timer error: " << ec.message() << endl;
                close_all();
            }
        }
    );
}

void client::send_get_info()
{
    if (state != state_type::wait_friend)
    {
        return;
    }

    server_buf = "get_info " + friend_name + "\r\n";
    async_write(server_socket, buffer(server_buf),
        [this](boost_error ec, size_t)
        {
            if (!ec)
            {
                start_read(handle_get_info);
            }
            else
            {
                cout << "get_info error: " << ec.message() << endl;
                close_all();
            }
        }
    );
}

void client::handle_get_info(string answer)
{
    string title = get_token(&answer);
    if (title != "info")
    {
        cout << "get_info invalid answer title" << endl;
        send_get_list();
        return;
    }

    if (state != state_type::wait_friend)
    {
        return;
    }

    string private_address = get_token(&answer);
    string private_port = get_token(&answer);
    string public_address = get_token(&answer);
    string public_port = get_token(&answer);

    tcp::endpoint private_endpoint = tcp::endpoint{
            ip::address::from_string(private_address),
            static_cast<uint16_t>(stoi(private_port))};
    tcp::endpoint public_endpoint = tcp::endpoint{
            ip::address::from_string(public_address),
            static_cast<uint16_t>(stoi(public_port))};

    socket_ptr private_socket = make_shared<tcp::socket>(service);
    socket_ptr public_socket = make_shared<tcp::socket>(service);

    private_socket->async_connect(private_endpoint,
        [this, private_socket](boost_error ec)
        {
            if (!ec)
            {
                cout << "communication started on private endpoint" << endl;
                activate_commutation(private_socket);
            }
            else
            {
                cout << "friend private connection error: " <<
                        ec.message() << endl;
            }
        }
    );
    public_socket->async_connect(public_endpoint,
        [this, public_socket](boost_error ec)
        {
            if (!ec)
            {
                cout << "communication started on public endpoint" << endl;
                activate_commutation(public_socket);
            }
            else
            {
                cout << "friend public connection error: " <<
                        ec.message() << endl;
            }
        }
    );
}

void client::activate_commutation(socket_ptr s)
{
    if (state == state_type::communicate_friend)
    {
        return;
    }

    if (!is_active_client())
    {
        available_sockets.push_back(s);

        auto send_confirm =
            [self = shared_from_this(), s, buf = make_shared<string>()]
            {
                *buf = "confirm_activation\r\n";
                async_write(*s, buffer(*buf),
                    [self, buf](boost_error ec, size_t)
                    {
                        if (ec)
                        {
                            cout << "write activation confirm error" << endl;
                            self->close_all();
                        }
                    }
                );
            };

        auto handler =
            [self = shared_from_this(), s, send_confirm](string answer)
            {
                if (answer == "activate")
                {
                    for (auto so : self->available_sockets)
                    {
                        if (so != s)
                        {
                            so->close();
                        }
                    }
                    self->available_sockets.clear();
                    send_confirm();
                    self->start_commutation(s);
                }
                else
                {
                    cout << "invalid activate command" << endl;
                    self->close_all();
                }
            };

        auto buf = make_shared<vector<char>>();
        buf->resize(MAX_READ_SERVER_BUF_SIZE);
        async_read(*s, buffer(*buf),
                   [self = shared_from_this(), buf]
                   (boost_error ec, size_t bytes)
                   { return self->read_complete(*buf, ec, bytes); },
                   [self = shared_from_this(), buf, handler]
                   (boost_error ec, size_t bytes)
                   { self->read(*buf, handler, ec, bytes); }
        );
    }

    if (state != state_type::wait_friend)
    {
        return;
    }

    state = state_type::connect_friend;
    if (is_active_client())
    {
        auto handler =
            [self = shared_from_this(), s](string answer)
            {
                if (answer == "confirm_activation")
                {
                    self->start_commutation(s);
                }
                else
                {
                    cout << "invalid confirm activation command" << endl;
                    self->close_all();
                }
            };

        auto read_confirm =
            [self = shared_from_this(), s, handler]
            {
                auto buf = make_shared<vector<char>>();
                buf->resize(MAX_READ_SERVER_BUF_SIZE);
                async_read(*s, buffer(*buf),
                           [self, buf]
                           (boost_error ec, size_t bytes)
                           { return self->read_complete(*buf, ec, bytes); },
                           [self, buf, handler]
                           (boost_error ec, size_t bytes)
                           { self->read(*buf, handler, ec, bytes); }
                );
            };

        auto buf = make_shared<string>();
        *buf = "activate\r\n";
        async_write(*s, buffer(*buf),
            [self = shared_from_this(), s, buf, read_confirm]
            (boost_error ec, size_t)
            {
                if (!ec)
                {
                    read_confirm();
                }
                else
                {
                    cout << "write activate error" << endl;
                    self->close_all();
                }
            }
        );
    }
}

void client::start_commutation(client::socket_ptr s)
{
    friend_active_socket = s;
    state = state_type::communicate_friend;

    cout << "communication started" << endl;
    do_friend_read();
}

void client::do_friend_read()
{
    using namespace std::placeholders;
    async_read(*friend_active_socket, buffer(friend_read_buf),
               bind(&client::read_complete_from_friend,
                    shared_from_this(), _1, _2),
               bind(&client::read_from_friend, shared_from_this(),
                    [self = shared_from_this()](string message)
                    { return self->handle_friend_message(move(message)); },
                    _1, _2));
}

void client::handle_friend_message(string message)
{
    string title = get_token(&message);
    string name = get_token(&message);
    string text = move(message);
    if (title == "message" && !name.empty() && !text.empty())
    {
        cout << ">> " << name << ": " << text << endl;
        do_friend_read();
    }
    else
    {
        cout << "invalid input message" << endl;
        close_all();
    }
}

void client::do_friend_write()
{
    async_write(*friend_active_socket, buffer(output_messages.front()),
        [this](boost_error ec, size_t)
        {
            if (!ec)
            {
                output_messages.pop();
                if (!output_messages.empty())
                {
                    do_friend_write();
                }
            }
            else
            {
                cout << "write to friend error: " << ec.message() << endl;
                close_all();
            }
        }
    );
}

bool client::start_acceptor()
{
    boost_error ec;
    acceptor.open(private_endpoint.protocol(), ec);
    if (ec)
    {
        cout << "acceptor open error: " << ec.message() << endl;
        close_all();
        return false;
    }
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                        ec);
    if (ec)
    {
        cout << "acceptor so_reuseaddress error: " << ec.message() << endl;
        close_all();
        return false;
    }
    acceptor.bind(private_endpoint, ec);
    if (ec)
    {
        cout << "acceptor bind error: " << ec.message() << endl;
        close_all();
        return false;
    }
    acceptor.listen();

    socket_ptr friend_server_socket = make_shared<tcp::socket>(service);
    acceptor.async_accept(*friend_server_socket,
        [this, friend_server_socket](boost_error ec)
        {
            if (!ec)
            {
                cout << "communication accepted" << endl;
                activate_commutation(friend_server_socket);
            }
            else
            {
                cout << "accept friend error: " << ec.message() << endl;
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
               bind(&client::read_complete_from_server,
                    shared_from_this(), _1, _2),
               bind(&client::read_from_server,
                    shared_from_this(),
                    [self = shared_from_this(), handler](std::string answer)
                    { ((*self).*handler)(move(answer)); },
                    _1, _2)
    );
}

template <typename Buffer>
size_t client::read_complete(const Buffer &buf, boost::system::error_code ec,
                             size_t bytes)
{
    if (ec)
    {
        return 0;
    }

    auto end = buf.cbegin() + bytes;
    auto start_it = find_if(buf.cbegin(), end,
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

size_t client::read_complete_from_server(boost_error ec, size_t bytes)
{
    if (!ec)
    {
        return read_complete(server_read_buf, ec, bytes);
    }
    else
    {
        cout << "read from server error: " << ec.message() << endl;
        close_all();
    }
}

size_t client::read_complete_from_friend(boost_error ec, size_t bytes)
{
    if (!ec)
    {
        return read_complete(friend_read_buf, ec, bytes);
    }
    else
    {
        cout << "read from friend error: " << ec.message() << endl;
        close_all();
    }
}

template <typename Buffer>
void client::read(const Buffer &buf,
                  std::function<void (string)> handler,
                  boost_error ec, size_t bytes)
{
    if (ec)
    {
        return;
    }

    auto end = buf.begin() + bytes;
    auto start_it = find_if(buf.begin(), end,
                            [](char c){ return !isspace(c); });
    auto finish_it = find_if(next(start_it), end,
                             [](char c){ return isspace(c) && c != ' '; });

    string request{start_it, finish_it};
    handler(move(request));
}

void client::read_from_server(function<void (string)> handler,
                              boost_error ec, size_t bytes)
{
    if (!ec)
    {
        read(server_read_buf, handler, ec, bytes);
    }
    else
    {
        cout << "read from server error: " << ec.message() << endl;
        close_all();
    }
}

void client::read_from_friend(std::function<void (string)> handler,
                              boost_error ec, size_t bytes)
{
    if (!ec)
    {
        read(friend_read_buf, handler, ec, bytes);
    }
    else
    {
        cout << "read from friend error: " << ec.message() << endl;
        close_all();
    }
}

void client::close_all()
{
    server_socket.close();
    acceptor.close();
    for (auto s : available_sockets)
    {
        if (s)
        {
            s->close();
        }
    }
    if (friend_active_socket)
    {
        friend_active_socket->close();
    }
}

string client::to_string(tcp::endpoint endpoint)
{
    return endpoint.address().to_string() + " " + ::to_string(endpoint.port());
}

string client::get_token(string *s)
{
    size_t space_index = s->find_first_of(' ');
    size_t token_index = space_index == string::npos ? s->length() :
                                                       space_index;

    string res = s->substr(0, token_index);
    if (token_index != s->length())
    {
        *s = s->substr(token_index + 1);
    }
    else
    {
        s->clear();
    }
    return res;
}

#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <array>
#include <vector>
#include <memory>
#include <functional>
#include <boost/asio.hpp>

class client : public std::enable_shared_from_this<client>
{
    client(std::string name, std::string server_ip, uint16_t server_port,
           std::string friend_name);

public:
    using ptr = std::shared_ptr<client>;
    static ptr create(std::string name,
                      std::string server_ip, uint16_t server_port,
                      std::string friend_name);

    void run();
    void write(std::string text);

private:
    boost::asio::io_service service;
    boost::asio::ip::tcp::socket server_socket;
    std::string name;
    boost::asio::ip::tcp::endpoint server_endpoint ;
    std::string server_buf;
    static constexpr size_t MAX_READ_SERVER_BUF_SIZE = 1024;
    std::array<char, MAX_READ_SERVER_BUF_SIZE> server_read_buf;

    std::string friend_name;
    boost::asio::ip::tcp::socket friend_server_socket;
    std::vector<boost::asio::ip::tcp::socket> friend_client_sockets;

    boost::asio::ip::tcp::endpoint private_endpoint;
    boost::asio::ip::tcp::acceptor acceptor;

    void open_connection();

    void send_connect();
    void handle_connect(std::string answer);
    void send_get_list();
    void handle_get_list(std::string answer);

    bool start_acceptor();
    void fill_private_endpoint();

    void start_read(void(client::*handler)(std::string));
    size_t read_complete(boost::system::error_code ec, size_t bytes);
    void read(std::function<void (std::string)> handler,
              boost::system::error_code ec, size_t bytes);
    void close_all();

    static std::string to_string(boost::asio::ip::tcp::endpoint endpoint);
};

#endif // CLIENT_H

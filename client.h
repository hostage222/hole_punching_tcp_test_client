#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <array>
#include <vector>
#include <queue>
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
    void write(const std::string &text);

private:
    boost::asio::io_service service;
    boost::asio::ip::tcp::socket server_socket;
    std::string name;
    boost::asio::ip::tcp::endpoint server_endpoint ;
    std::string server_buf;
    static constexpr size_t MAX_READ_SERVER_BUF_SIZE = 1024;
    std::array<char, MAX_READ_SERVER_BUF_SIZE> server_read_buf;

    std::string friend_name;
    boost::asio::deadline_timer friend_repeat_timer;

    using socket_ptr = std::shared_ptr<boost::asio::ip::tcp::socket>;
    boost::asio::ip::tcp::endpoint private_endpoint;
    boost::asio::ip::tcp::acceptor acceptor;
    enum class state_type {wait_friend, connect_friend, communicate_friend}
        state = state_type::wait_friend;
    std::vector<socket_ptr> available_sockets;
    socket_ptr friend_active_socket;
    std::array<char, MAX_READ_SERVER_BUF_SIZE> friend_read_buf;
    static constexpr size_t MAX_SAVED_OUTPUT_MESSAGES = 16;
    std::queue<std::string> output_messages;

    void open_connection();

    void send_connect();
    void handle_connect(std::string answer);
    void send_get_list();
    void handle_get_list(std::string answer);
    void send_get_info();
    void handle_get_info(std::string answer);

    void activate_commutation(socket_ptr s);
    void activate_socket(socket_ptr s);

    void start_commutation(socket_ptr s);
    void do_friend_read();
    void handle_friend_message(std::string message);
    void do_friend_write();

    bool start_acceptor();
    void fill_private_endpoint();

    void start_read(void(client::*handler)(std::string));
    template <typename Buffer>
    size_t read_complete(const Buffer &buf, boost::system::error_code ec,
                         size_t bytes);
    size_t read_complete_from_server(boost::system::error_code ec,
                                     size_t bytes);
    size_t read_complete_from_friend(boost::system::error_code ec,
                                     size_t bytes);
    template <typename Buffer>
    void read(const Buffer &buf, std::function<void (std::string)> handler,
              boost::system::error_code ec, size_t bytes);
    void read_from_server(std::function<void(std::string)> handler,
                          boost::system::error_code ec, size_t bytes);
    void read_from_friend(std::function<void(std::string)> handler,
                          boost::system::error_code ec, size_t bytes);
    void close_all();
    //categorize clients to start communication
    bool is_active_client() { return !friend_name.empty(); }

    static std::string to_string(boost::asio::ip::tcp::endpoint endpoint);
    static std::string get_token(std::string *s);
};

#endif // CLIENT_H

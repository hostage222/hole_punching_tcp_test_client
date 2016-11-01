#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <exception>
#include <thread>
#include <string>

#include "client.h"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 4 && argc != 5)
    {
        cerr << "Usage: test_client <own_name> <server_ip> <server_port> "
                "[friend_name]" << endl;
        return -1;
    }

    client::ptr cl = client::create(argv[1],
            argv[2], static_cast<uint16_t>(atoi(argv[3])),
            argc == 5 ? argv[4] : "");

    /*thread t{
        [&cl]
        {
            try
            {
                cl->run();
            }
            catch (exception &e)
            {
                cerr << "Exception: " << e.what() << endl;
            }
            catch (...)
            {
                cerr << "Undefined exception" << endl;
            }
        }
    };

    while (true)
    {
        string message;
        getline(cin, message);
        cl->write(message);
    }*/

    cl->run();

    return 0;
}

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <exception>
#include <thread>
#include <string>
#include <atomic>

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

    atomic<bool> in_work{true};
    thread t{
        [&cl, &in_work]
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
            cout << "Press <RETURN> to close this window..." << endl;
            in_work = false;
        }
    };
    t.detach();

    while (in_work)
    {
        string message;
        getline(cin, message);
        cl->write(message);
    }

    return 0;
}

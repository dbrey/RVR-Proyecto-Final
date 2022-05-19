#include <thread>
#include "Chat.h"

int main(int argc, char **argv)
{
    ChatClient ec(argv[1], argv[2], argv[3]);

    std::thread net_thread([&ec](){ ec.net_thread(); });

    ec.login();

    // Repartimos inicialmente 7 cartas al jugador
    for(int i = 0; i < 7; i++)
    {
        ec.myCards.push_back(ec.generateCard());
    }

    ec.input_thread();
}


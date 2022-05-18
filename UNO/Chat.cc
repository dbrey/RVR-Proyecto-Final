#include "Chat.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

void ChatMessage::to_bin()
{
    alloc_data(MESSAGE_SIZE);

    memset(_data, 0, MESSAGE_SIZE);

    //Serializar los campos type, nick y message en el buffer _data
    char * tmp = _data;
    memcpy(tmp, &type, sizeof(uint8_t));
    tmp += sizeof(uint8_t);
    memcpy(tmp, nick.c_str(), 8 * sizeof(char));
    tmp += 8 * sizeof(char);
    memcpy(tmp, message.c_str(), 80 * sizeof(char));
}

int ChatMessage::from_bin(char * bobj)
{
    alloc_data(MESSAGE_SIZE);

    memcpy(static_cast<void *>(_data), bobj, MESSAGE_SIZE);

    //Reconstruir la clase usando el buffer _data
    char * tmp = _data;
    memcpy(&type, tmp, sizeof(uint8_t));
    tmp += sizeof(uint8_t);
    nick = tmp;
    tmp += 8 * sizeof(char);
    message = tmp;

    return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

void ChatServer::do_messages()
{
    while (true)
    {
        /*
         * NOTA: los clientes están definidos con "smart pointers", es necesario
         * crear un unique_ptr con el objeto socket recibido y usar std::move
         * para añadirlo al vector
         */

        //Recibir Mensajes en y en función del tipo de mensaje
        ChatMessage message;
        Socket *messageSocket = new Socket(socket);
        socket.recv(message, messageSocket);

        // - LOGIN: Añadir al vector clients
        if(message.type == message.LOGIN){
            std::cout << "LOGIN " << *messageSocket << "\n";
            std::unique_ptr<Socket> uptr = std::make_unique<Socket>(*messageSocket);
            messageSocket = nullptr;
            clients.push_back(std::move(uptr));
        }

        // - LOGOUT: Eliminar del vector clients
        else if(message.type == message.LOGOUT){
            std::cout << "LOGOUT " << *messageSocket << "\n";
            auto it = clients.begin();
            while(it != clients.end() && !(**it == *messageSocket)) ++it;
            if(it == clients.end()) std::cout << "Client not found\n";
            else clients.erase(it);
        }

        // - MESSAGE: Reenviar el mensaje a todos los clientes (menos el emisor)
        else if(message.type == message.MESSAGE){
            // Aumenta turn //////////////////////////////////////////////////////////////////////////////////////////
            std::cout << "MESSAGE " << *messageSocket << "\n";
            for(auto it = clients.begin(); it != clients.end(); ++it){
                if(!(**it == *messageSocket)) socket.send(message, **it);
                // message.turn se pondrá a true o false dependiendo de si it es clients[turn] ///////////////////////
            }
        }
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

void ChatClient::login()
{
    std::string msg;

    ChatMessage em(nick, msg);
    em.type = ChatMessage::LOGIN;

    socket.send(em, socket);
}

void ChatClient::logout()
{
    // Completar
    std::string msg;

    ChatMessage em(nick, msg);
    em.type = ChatMessage::LOGOUT;

    socket.send(em, socket);
}

void ChatClient::input_thread()
{
    bool chat = true;
    while (chat)
    {
        // Leer input (flechas o enter) ////////////////////////////////////////////////////////////////////////

        // Leer stdin con std::getline
        std::string msg;
        std::getline(std::cin, msg);

        if(msg == "exit"){
            chat = false;
        }
        else{
            // Enviar al servidor usando socket
            ChatMessage em(nick, msg);
            em.type = ChatMessage::MESSAGE;

            socket.send(em, socket);
        }
    }
    logout();
}

void ChatClient::net_thread()
{
    ChatMessage em;
    Socket *mSocket = new Socket(socket);
    while(true)
    {
        //Recibir Mensajes de red
        socket.recv(em, mSocket);

        // Si la carta es +2 llama 2 veces a CogerCarta ///////////////////////////////////////////////////////////////
        // Si mensaje == END -> playing = false ///////////////////////////////////////////////////////////////////////

        //Mostrar en pantalla el mensaje de la forma "nick: mensaje"
        std::cout << em.nick << ": " << em.message << "\n";
    }
}


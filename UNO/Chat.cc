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

    memcpy(tmp, &type, sizeof(uint8_t)); // Nº Carta
    tmp += sizeof(uint8_t);

    memcpy(tmp, &type, sizeof(uint8_t)); // Color
    tmp += sizeof(uint8_t);

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

    memcpy(&number, tmp, sizeof(uint8_t)); // Nº Carta
    tmp += sizeof(uint8_t);

    memcpy(&color, tmp, sizeof(uint8_t)); // Nº Color
    tmp += sizeof(uint8_t);

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

            // En caso de que el ultimo jugador se vaya en su turno, nos aseguramos de que se pase el turno al siguiente jugador
            // (Ejemplo turno 4, pero jugador 4 se va. Si pasa a haber 3 jugadores solo, entonces pasamos automaticamente el turno)
            if(turn >= clients.size())
            {
                turn = 0;
                clients[turn]->setTurn(true);
            }

            if(clients.size() == 1)
            {
                turn = -1;
            }
        }

        // - MESSAGE: Reenviar el mensaje a todos los clientes (menos el emisor)
        else if(message.type == message.MESSAGE){
            
            // Comunicamos al jugador actual que su turno se ha terminado y que le toca al siguiente
            // Tener el turno desactivado simplemente impide que pueda enviar un tipo MESSAGE pero puede salir de la partida
            clients[turn]->setTurn(false);
            turn++;
            if(turn >= clients.size())
            {
                turn = 0;
            }
            clients[turn]->setTurn(true);
            //------------------------------------------------------------------


           
            for(auto it = clients.begin(); it != clients.end(); ++it){
                if(!(**it == *messageSocket)) socket.send(message, **it);                
            }
        }

        else if(message.type == message.END)
        {
            std::cout << "Ha ganado el jugador: " << message.nick << "\n";
            turn = -1; // Para indicar que la partida ha empezado

        }
        else if(message.type == message.BEGIN)
        {
            // Si no hay una partida empezada y hay al menos 2 jugadores
            if(clients.size() > 1 && turn == -1)
            {
                turn = 0;
                /*clients[turn]->setTurn(true);
                clients[0].get()->setTurn(true);*/

                //auto it = clients.begin() + turn;
                

                
                std::cout << "El juego ha empezado ! " << "\n";
                for(auto it = clients.begin(); it != clients.end(); ++it){
                    socket.send(message, **it);                
            }

            // Meter una carta comun en topCard
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
    
    ///////////////////////////////////////////////////////////////// Código provisional para ver que funciona:
    /*topCard = generateCard();
    for(int i = 0; i < 7; i++)
    {
        myCards.push_back(generateCard());
    }*/
    std::cout << "Acabas de unirte a la partida " << nick << "\n";
    printRules();
    //printGame();
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
        std::string msg = "";
        bool cardSelected = false;
        while(!cardSelected) {
            std::getline(std::cin, msg);
            if(msg == "d"){
                if(cardPointer < myCards.size() - 1) cardPointer++;
            }
            else if(msg == "a"){
                if(cardPointer > 0) cardPointer--;
            }
            else if(msg == "exit" || msg == "start"){
                cardSelected = true;
            }
            else if(socket.getTurn() && (msg == "s" || msg == "uno")){
                if(throwCard()) cardSelected == true;
            }
            printGame();
        }

        // Leer stdin con std::getline
        //std::string msg;
        //std::getline(std::cin, msg);

        if(msg == "exit"){
            chat = false;
        }
        else if(!playing && msg == "start")
        {
            // Comunicamos que la partida ha empezado
            ChatMessage em(nick, msg);
            em.type = ChatMessage::BEGIN;
            
            // Generamos un topcard random
            topCard = generateCard();
            em.color = topCard.color;
            em.number = topCard.number;

            socket.send(em, socket);
        }
        else{
            
            
            // Si es el turno del jugador, enviamos mensaje
            if(socket.getTurn())
            {
               
                ChatMessage em(nick, msg);
                ///// Si la ultima carta se lanza, mandar un mensaje tipo END
                if(myCards.size() == 0){
                    em.type = ChatMessage::END;
                }
                else
                {
                    em.type = ChatMessage::MESSAGE;
                }
                
                em.color = topCard.color;
                em.number = topCard.number;
                socket.send(em, socket);
            }
            
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

        // Si la carta es +2 llama 2 veces a CogerCarta 
        if(topCard.number == 10) // Cuidado que esto no se llame infinito hasta enviar la siguiente carta
        {
            myCards.push_back(generateCard());
            myCards.push_back(generateCard());
        }


        if(em.type == ChatMessage::MESSAGE)
        {
            topCard.color = em.color;
            topCard.number = em.number;
            printGame();
        }        
        else if(em.type == ChatMessage::END) // Si termina el juego, el jugador no puede mandar mas cartas aunque lo intente
        {
            playing = false;
        }
        else if(em.type == ChatMessage::BEGIN) // Si el juego empieza, se le da una baraja inicial y se pone la primera carta en el tablero
        {
            playing = true;
            startGame();
            topCard.color = em.color;
            topCard.number = em.number;
            printGame();
        }
    }
}

card ChatClient::generateCard()
{
    // 0-11 value of the card, 0-3 value of the color
    card aux = {rand()%12,rand()%4};
    return aux;
}

bool ChatClient::throwCard()
{
    if(checkCurrentCard(myCards[cardPointer]))
    {
        topCard = myCards[cardPointer];
        std::vector<card>::iterator it = myCards.begin() + cardPointer;
        myCards.erase(it);
        return true;
    }
    return false;
}

bool ChatClient::checkCurrentCard(card nextCard)
{
    if(nextCard.number == 11)
    {
        
        // Seleccionamos color preguntandole por input (y asi, automaticamente se lanza con el color ya elegido)
        std::cout << "Selecciona un color (azul, amarillo, rojo, verde) :" << "\n";

        std::string msg;
        std::getline(std::cin, msg);

        if(msg == "azul")
        {
            nextCard.color = 0;
        }
        else if(msg == "amarillo")
        {
            nextCard.color = 1;
        }
        else if(msg == "rojo")
        {
            nextCard.color = 2;
        }
        else if(msg == "verde")
        {
            nextCard.color = 3;
        }
        return true;
    }
    // If color or number is the same, player can throw the card
    else if((topCard.color == nextCard.color) || (topCard.number == nextCard.number))
    {
        return true;
    }
    
    return false;
}

void ChatClient::printRules(){
    std::cout << "REGLAS:\n";
    std::cout << " - El objetivo es lanzar una carta que coincida en número o color con la del mazo\n";
    std::cout << " - Utiliza los cursores para desplazarte por tu mazo y espacio para utilizar la carta seleccionada\n";
}

void ChatClient::printGame(){
    system("clear"); // Comando para borrar la pantalla

    std::cout << "MAZO: ";
    topCard.print(); 
    std::cout << "\n\n";

    for(card c : myCards) c.print();
    std::cout << "\n";
    for(int i = 0; i < myCards.size(); i++) {
        if(i != cardPointer) std::cout << "   ";
        else std::cout << "*  ";
    }
    std::cout << "\n";
}

void ChatClient::startGame()
{
    // Si el jugador tenia cartas de antes, se las quitamos antes de empezar el juego
    if(myCards.size() > 0)
    {
        myCards.clear();
    }

    // Repartimos inicialmente 7 cartas al jugador
    for(int i = 0; i < 7; i++)
    {
        myCards.push_back(generateCard());
    }
}
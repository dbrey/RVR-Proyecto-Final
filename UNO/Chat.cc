#include "Chat.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

void ChatMessage::to_bin()
{
    alloc_data(MESSAGE_SIZE);

    memset(_data, 0, MESSAGE_SIZE);

    //Serializar los campos type, nick y message en el buffer _data
    char * tmp = _data;
    memcpy(tmp, &type, sizeof(uint8_t)); // Tipo de mensaje
    tmp += sizeof(uint8_t);

    memcpy(tmp, nick.c_str(), 8 * sizeof(char)); // Nick
    tmp += 8 * sizeof(char);

    memcpy(tmp, &number, sizeof(uint8_t)); // Nº Carta
    tmp += sizeof(uint8_t);

    memcpy(tmp, &color, sizeof(uint8_t)); // Color
    tmp += sizeof(uint8_t);

    memcpy(tmp, &newTurn, sizeof(bool)); // Turno
    tmp += sizeof(bool);

    memcpy(tmp, message.c_str(), 80 * sizeof(char)); // Mensaje
}

int ChatMessage::from_bin(char * bobj)
{
    alloc_data(MESSAGE_SIZE);

    memcpy(static_cast<void *>(_data), bobj, MESSAGE_SIZE);

    //Reconstruir la clase usando el buffer _data
    char * tmp = _data;
    memcpy(&type, tmp, sizeof(uint8_t)); // Tipo de mensaje
    tmp += sizeof(uint8_t);

    nick = tmp; // Nick
    tmp += 8 * sizeof(char); 

    memcpy(&number, tmp, sizeof(uint8_t)); // Nº Carta
    tmp += sizeof(uint8_t);

    memcpy(&color, tmp, sizeof(uint8_t)); // Nº Color
    tmp += sizeof(uint8_t);

    memcpy(&newTurn, tmp, sizeof(bool)); // Turno
    tmp += sizeof(bool);
    
    message = tmp; // Mensaje

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
        message.newTurn = false;
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
            int i = 0; // Indice del jugador a eliminar
            while(it != clients.end() && !(**it == *messageSocket)){
                ++it;
                ++i;
            }

            if(it == clients.end()) std::cout << "Client not found\n";
            else { // Borramos al jugador manteniendo los turnos
                (*it).reset();
                clients.erase(it);
                

                if(clients.size() == 1) // Si solo queda un jugador, le sacamos de la partida
                {
                    turn = -1;
                    message.newTurn = false;
                    socket.send(message, *clients[0]);
                }
                else if(turn == i){ // Si le tocaba al que se va, le toca al que ocupará su posición
                    message.newTurn = true;
                    if(turn < clients.size()) socket.send(message, *clients[turn]); // No era el último
                    else socket.send(message, *clients[0]); // Era el último
                }
                else if(turn > i) turn--; // El que tenía turno ha retrocedido una posición
                
            }
        }

        else if(message.type == message.MESSAGE){
            
            // Comunicamos al siguiente jugador que le toca
            // Tener el turno desactivado simplemente impide que pueda enviar un tipo MESSAGE pero puede 
            // salir de la partida y moverse para seleccionar la siguiente carta
            
            turn = (turn + 1) % clients.size();          
            for(auto it = clients.begin(); it != clients.end(); ++it)
            {
                if(**it == *clients[turn])
                {
                    message.newTurn = true;
                    socket.send(message, **it); // Le comunicamos que es el turno del jugador, junto con la carta en la mesa
                    message.newTurn = false;
                }
                else 
                {
                    socket.send(message, **it);          
                }
            }
            
        }

        else if(message.type == message.END)
        {
            std::cout << "Ha ganado el jugador: " << message.nick << "\n";
            turn = -1; // Para indicar que la partida ha terminado

            for(auto it = clients.begin(); it != clients.end(); ++it){
                    socket.send(message, **it);                
            }
        }
        else if(message.type == message.BEGIN)
        {
            // Si no hay una partida empezada y hay al menos 2 jugadores
            if(clients.size() > 1 && turn == -1)
            {
                
                turn = 0;
                message.newTurn = true;
                auto it = clients.begin();
                socket.send(message, **it);
                
                message.newTurn = false;    
                std::cout << "El juego ha empezado ! " << "\n";
                for(it = it+1; it != clients.end(); ++it){
                    socket.send(message, **it);                
                }

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
    
    printRules();
}

void ChatClient::logout()
{
    // Completar
    std::string msg;

    ChatMessage em(nick, msg);
    em.type = ChatMessage::LOGOUT;

    socket.send(em, socket);
    printExit();
}

void ChatClient::input_thread()
{
    bool chat = true;
    while (chat)
    {

        std::string msg = "";
        std::string error;

        bool choosing = true;
        while(choosing) {
            error = "";
            std::getline(std::cin, msg);

            // SUMMARY
            // El jugador se mueve mandando mensajes 'a' y 'd'
            // EL jugador manda la carta mandando un mensaje 's' o si solo le queda 2 cartas, "uno"
            // Si el jugador selecciona la "carta" R (robar), roba 1 carta
            // Todo esto solo si es el turno del jugador

            if(msg == "start" || msg == "exit"){
                choosing = false;
            }
            else if(msg == "d"){
                if(cardPointer < myCards.size()) cardPointer++;
            }
            else if(msg == "a"){
                if(cardPointer > 0) cardPointer--;
            }
            else if(yourTurn && (msg == "s" || msg == "uno")){
                if(cardPointer == myCards.size()){
                    if(!tryGettingCard()) error = "Puedes echar, no puedes robar";
                }
                else if(throwCard()) choosing = false;
                else error = "Imposible usar esta carta";
            }
            else {
                if(yourTurn) error = "Comando no reconocido";
                else error = "No es tu turno";
            }

            if(playing) printGame(error);
            
        }


        if(!playing && msg == "start")
        {
            // Comunicamos que la partida ha empezado
            ChatMessage em(nick, msg);
            em.type = ChatMessage::BEGIN;
            
            // Generamos un topcard random
            card top = generateNumberCard();
            if(top.number > 9) { top.number = 9;} // Para evitar que salga cambio de color
            em.color = top.color;
            em.number = top.number;

            socket.send(em, socket);
        }
        else if(msg == "exit"){
            chat = false;
        }
        else{         
            
            // Si es el turno del jugador, enviamos mensaje
            if(yourTurn)
            {
                ChatMessage em(nick, msg);
                // Si la ultima carta se lanza, mandar un mensaje tipo END
                if(myCards.size() == 0){
                    em.type = ChatMessage::END;
                }
                else
                {
                    em.type = ChatMessage::MESSAGE;
                    if(myCards.size() == 1 && em.message != "uno"){
                        getCard(2);
                    }
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
        // Recibir Mensajes de red
        socket.recv(em, mSocket);

        if(em.type == ChatMessage::MESSAGE)
        {
            topCard.color = em.color;
            topCard.number = em.number;
            yourTurn = em.newTurn;
            if(yourTurn) // Si le toca roba dos cartas
            {
                if(topCard.number == 10)
                {
                    getCard(2);
                }
                else if(topCard.number == 12)
                {
                    getCard(4);
                }
                
            }

            printGame("");
        }
        if(em.type == ChatMessage::LOGOUT) // Si alguien se sale y es un solo jugador, lo sacamos de la partida
        {
            if(!em.newTurn)
            {
                yourTurn = false;
                playing = false;
                printRules();
            }
            else
            {
                yourTurn = em.newTurn;
                printGame("El jugador al que le tocaba ha abandonado la partida");
            }
            
            
        }     
        else if(em.type == ChatMessage::END) // Si termina el juego, el jugador no puede mandar mas cartas aunque lo intente
        {
            playing = false;
            printEndGame(em.nick);
        }
        else if(em.type == ChatMessage::BEGIN) // Si el juego empieza, se le da una baraja inicial y se pone la primera carta en el tablero
        {
            playing = true;
            startGame();
            topCard.color = em.color;
            topCard.number = em.number;
            yourTurn = em.newTurn;
            printGame("");
        }
    }
}

card ChatClient::generateNumberCard(){
    card aux = {rand()&10, rand()%4};
    return aux;
}

card ChatClient::generateCard()
{
    // PROBABILIDADES
    // Cada número: 2/23     +2: 2/23     Cambio color: 1/23    +4 = 1/23
    /*int prob = rand()%24;
    card c;
    if(prob < 20) c = generateNumberCard(); // Número
    else if(prob < 22) c = {10, rand()%4}; // +2
    else if(prob < 23) c = {11, 4}; // Cambio de color
    else c = {12,4};*/ // +4
    
    
    card c = {rand()%13,rand()%4};
    if(c.number == 11 || c.number == 12)
    {
        c.color = 4;
    }

    return c;
}

bool ChatClient::throwCard()
{
    if(checkCurrentCard(&myCards[cardPointer])) // Comprueba si puede lanzar la carta
    {
        // Si puede, sustituye la carta de la mesa, y se elimina del mazo
        topCard = myCards[cardPointer];
        std::vector<card>::iterator it = myCards.begin() + cardPointer;
        myCards.erase(it);
        cardPointer = 0;
        return true;
    }
    return false;
}

bool ChatClient::checkCurrentCard(card* nextCard)
{
    if(nextCard->number == 11 || nextCard->number == 12)
    {
        
        // Seleccionamos color preguntandole por input (y asi, automaticamente se lanza con el color ya elegido)
        std::cout << "Selecciona un color (azul, amarillo, rojo, verde) :" << "\n";

        std::string msg;
        std::getline(std::cin, msg);
        while(msg != "azul" && msg != "amarillo" && msg != "rojo" && msg != "verde"){
            printGame("Color no reconocido");
            std::cout << "Selecciona un color (azul, amarillo, rojo, verde) :" << "\n";
            std::getline(std::cin, msg);
        }

        if(msg == "azul")
        {
            nextCard->color = 0;
        }
        else if(msg == "amarillo")
        {
            nextCard->color = 1;
        }
        else if(msg == "rojo")
        {
            nextCard->color = 2;
        }
        else if(msg == "verde")
        {
            nextCard->color = 3;
        }
        return true;
    }
    // Si el color o el numero son iguales, puede lanzar la carta
    else if((topCard.color == nextCard->color) || (topCard.number == nextCard->number))
    {
        return true;
    }
    
    return false;
}

void ChatClient::getCard(int n){
    for(int i = 0; i < n; i++){
        myCards.push_back(generateCard());
    }
}

bool ChatClient::tryGettingCard(){
    int i = 0;
    while(i < myCards.size() && myCards[i] != topCard && myCards[i].number != 11) ++i;
    
    if(i == myCards.size()){
        
        if(myCards.size() < 13)
        {
            getCard(1);
        }
        else
        {
            // Para que no robe infinitas cartas, si llega al limite de cartas, le damos un cambio de color
            card aux;
            aux.color = 4;
            aux.number = 11;
            myCards.push_back(aux);
        }
        
        return true;
    }
    return false;
}

void ChatClient::startGame()
{
    // Si el jugador tenia cartas de antes, se las quitamos antes de empezar el juego
    if(myCards.size() > 0)
    {
        myCards.clear();
    }

    getCard(7);
}

void ChatClient::printRules(){
    system("clear"); 
    std::cout << "Bienvenido al UNO \e[1m" << nick << "\e[0m\n\n";

    std::cout << "REGLAS:\n";
    std::cout << " - El objetivo es lanzar una carta que coincida en número o color con la del mazo\n";
    std::cout << " - \e[1ma\e[0m y \e[1md\e[0m para desplazarte por tus cartas\n";
    std::cout << " - \e[1ms\e[0m para utilizar la carta seleccionada\n";
    std::cout << " - \e[1muno\e[0m cuando te quedes con una única carta\n\n";

    std::cout << "\e[1mstart\e[0m para comenzar la partida\n\n";
}

void ChatClient::printEndGame(std::string winner){
    system("clear");
    std::cout << "Partida finalizada. Ganador: " << winner << "\n";
    std::cout << "start para volver a empezar" << "\n";
}

void ChatClient::printExit(){
    system("clear");
    std::cout << "Saliste de la partida" << "\n";
}

void ChatClient::printGame(std::string error){
    system("clear"); // Comando para borrar la pantalla

    std::cout << "MAZO: ";
    topCard.print(); 
    if(yourTurn) std::cout << "..............................Tu turno\n\n";
    else std::cout << "..............................No es tu turno\n\n";

    // Escribimos la parte superior de las cartas por color
    for(int i = 0; i< myCards.size()+1; i++)
    {   
        if(i == myCards.size())
        {
            std::cout << "\033[0m";
        }
        else
        {
            changeColor(myCards[i].color);
        }
       
        std::cout << "+---+  ";
    }

    std::cout << "\n";

    // Escribimos columnas de las por color
    for(int i = 0; i< myCards.size()+1; i++)
    {
       if(i == myCards.size())
        {
            std::cout << "\033[0m";
        }
        else
        {
            changeColor(myCards[i].color);
        }
       std::cout << "|   |  ";
    }

    std::cout << "\n";

    // Escribimos las columnas y el valor de las cartas con su color correspondiente
    for(int i = 0; i < myCards.size()+1; i++)
    {
        if(i == myCards.size())
        {
            std::cout << "\033[0m";
            std::cout << "| " << "R" << " |  "; // Robar carta
        }
        else
        {
            changeColor(myCards[i].color);
            if(myCards.at(i).number <10)
            {
                char c = myCards.at(i).number + '0';
                std::cout << "| " << c << " |  "; // Numeros del 0 al 9
            }
            else
            {
                if(myCards.at(i).number == 10 || myCards.at(i).number ==12)
                {
                    std::cout << "| " << "+" << " |  "; // +2 cartas
                }
                else if(myCards.at(i).number ==11)
                {
                    std::cout << "| " << "-" << " |  "; // Cambio de color
                }
                
            }
        }
    }

    std::cout << "\n";

    // Escribimos columnas de las por color
    for(int i = 0; i< myCards.size()+1; i++)
    {
        if(i == myCards.size())
        {
            std::cout << "\033[0m";
        }
        else
        {
            changeColor(myCards[i].color);
        }
       std::cout << "|   |  ";
    }

    std::cout << "\n";

    // Escribimos la parte inferior de las cartas por color
    for(int i = 0; i< myCards.size()+1; i++)
    {
        if(i == myCards.size())
        {
            std::cout << "\033[0m";
        }
        else
        {
            changeColor(myCards[i].color);
        }
       std::cout << "+---+  ";
    }

    std::cout << "\033[0m  "; // Restablecemos el color

    std::cout << "\n";
    for(int i = 0; i < myCards.size() + 1; i++) {
        if(i != cardPointer) std::cout << "       ";
        else std::cout << "  *  ";
    }
    std::cout << "\n";
    if(error != "") std::cout << "\033[0;31m" << error << "\033[0m\n";
}

void ChatClient::changeColor(int c)
{
    switch (c)
        {
        case 0:
            std::cout << "\033[0;94m";
            break;
        case 1:
            std::cout << "\033[0;93m";
            break;
        case 2:
            std::cout << "\033[0;91m";
            break;
        case 3:
            std::cout << "\033[0;92m";
            break;
        case 4:
            std::cout << "\033[0;95m";
            break;
        default:
            break;
        }

}
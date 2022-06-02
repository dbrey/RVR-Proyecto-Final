#include <string>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <memory>

#include "Serializable.h"
#include "Socket.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/**
 *  Mensaje del protocolo de la aplicación de Chat
 *
 *  +-------------------+
 *  | Tipo: uint8_t     | 0 (login), 1 (mensaje), 2 (logout)
 *  +-------------------+
 *  | Nick: char[8]     | Nick incluido el char terminación de cadena '\0'
 *  +-------------------+
 *  | Tipo: uint8_t     | Número de la carta (0-9, 10 es el +2 y 11 es el cambio de color)
 *  +-------------------+
 *  | Tipo: uint8_t     | Color de la carta (0 (azul), 1 (amarillo), 2 (rojo), 3 (verde))
 *  +-------------------+
 *  | Tipo: bool        | Le toca jugar o no
 *  +-------------------+
 *
 */

struct card {
    uint8_t number;
    uint8_t color;

    bool operator!=(const card& other)
    {
        return (other.number != number && other.color != color);
    }

    void print() {
        char c;

        if (number < 10) c = number + '0'; // Suma el ASCII number al ASCII '0'
        else if (number == 10 || number == 12) c = '+';
        else if (number == 11) c = '-';
        else if (number == 13) c = 'X';
        else if (number == 14) c = '#';

        switch (color)
        {
        case 0:
            std::cout << "\033[0;94m"; // Azul
            break;
        case 1:
            std::cout << "\033[0;93m"; // Amarillo
            break;
        case 2:
            std::cout << "\033[0;91m"; // Rojo
            break;
        case 3:
            std::cout << "\033[0;92m"; // Verde
            break;
        case 4:
            std::cout << "\033[0;95m"; // Cambio de color
            break;
        default:
            break;
        }

        // Diiujar la carta de forma especifica
        std::cout << "+---+ " << "\n";
        std::cout << "|   | " << "\n";
        std::cout << "| " << c << " | " << "\n";
        std::cout << "|   | " << "\n";
        std::cout << "+---+ " << "\n";

        std::cout << "\033[0m  ";
    }
};

class ChatMessage : public Serializable
{
public:
    static const size_t MESSAGE_SIZE = sizeof(char) * 80 + 2 * sizeof(uint8_t) + sizeof(bool);

    enum MessageType
    {
        LOGIN = 0,
        MESSAGE = 1,
        LOGOUT = 2,
        BEGIN = 3,
        END = 4
    };

    ChatMessage() {};

    ChatMessage(const std::string& n, const std::string& m) :nick(n), message(m) {};

    void to_bin();

    int from_bin(char* bobj);

    uint8_t type;

    std::string nick;
    uint8_t number = 0;
    uint8_t color = 0;
    bool newTurn = false; // Para comunicar si es el turno del jugador
    std::string message;
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/**
 *  Clase para el servidor de chat
 */
class ChatServer
{
public:
    ChatServer(const char* s, const char* p) : socket(s, p)
    {
        socket.bind();
    };

    /**
     *  Thread principal del servidor recive mensajes en el socket y
     *  lo distribuye a los clientes. Mantiene actualizada la lista de clientes
     */
    void do_messages();

private:
    /**
     *  Lista de clientes conectados al servidor de Chat, representados por
     *  su socket
     */
    std::vector<std::unique_ptr<Socket>> clients;

    /**
     * Le toca jugar a clients[turn]
     */
    int8_t turn = -1;

    /**
     *  Para revertir los turnos
     */
    bool reverse = false;

    /**
     * Socket del servidor
     */
    Socket socket;
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/**
 *  Clase para el cliente de chat
 */
class ChatClient
{
private:

    /**
     * Socket para comunicar con el servidor
     */
    Socket socket;

    /**
     * Nick del usuario
     */
    std::string nick;

    /**
     * Para saber si se está jugando o no
     */
    bool playing;

    /**
     * Para saber si te toca jugar o no
     */
    bool yourTurn = false;

    /**
     * Carta del centro
     */
    card topCard;

    /**
     * Para seleccionar cartas
     */
    uint8_t cardPointer = 0;

    /**
     *  Genera una carta de color y valor random (solo números)
     */
    card generateNumberCard();

    /**
     *  Genera una carta de color y valor random (números y especiales)
     */
    card generateCard();

    /**
     *  Lanza la carta (si es válida)
     */
    bool throwCard();

    /**
     *  Comprueba si la carta a lanzar es compatible
     */
    bool checkCurrentCard(card* nextCard);

    /**
     *  El jugador roba n cartas
     */
    void getCard(int n);

    /**
     *  Mira si el jugador puede robar una carta (y si eso la roba)
     */
    bool tryGettingCard();

    /**
     *  Al empezar el juego o unirse a una partida ya iniciada
     */
    void startGame();

    /**
     *  Imprime las reglas del juego
     */
    void printRules();

    /**
     *  Imprime pantalla fin de juego
     */
    void printEndGame(std::string winner);

    /**
     *  Imprime pantalla saliste del juego
     */
    void printExit();

    /**
     *  Cambia color para salida de texto por consola
     */
    void changeColor(int c);

    /**
     *  Imprime las reglas del juego
     */
    void printGame(std::string error);

public:
    /**
     * Vector de mis cartas
     */
    std::vector<card> myCards;

    /**
     * @param s dirección del servidor
     * @param p puerto del servidor
     * @param n nick del usuario
     */
    ChatClient(const char* s, const char* p, const char* n) :socket(s, p),
        nick(n) {
        srand((unsigned int)time(NULL)); // Semilla para números aleatorios de verdad (sino son siempre los mismos)
    };

    /**
     *  Envía el mensaje de login al servidor
     */
    void login();

    /**
     *  Envía el mensaje de logout al servidor
     */
    void logout();

    /**
     *  Rutina principal para el Thread de E/S. Lee datos de STDIN (std::getline)
     *  y los envía por red vía el Socket.
     */
    void input_thread();

    /**
     *  Rutina del thread de Red. Recibe datos de la red y los "renderiza"
     *  en STDOUT
     */
    void net_thread();
};


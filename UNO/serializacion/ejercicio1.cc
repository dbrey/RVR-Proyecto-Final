#include "Serializable.h"

#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

class Jugador: public Serializable
{
public:
    Jugador(const char * _n, int16_t _x, int16_t _y):x(_x),y(_y)
    {
        strncpy(name, _n, 80);
    };

    virtual ~Jugador(){};

    void to_bin()
    {
        int32_t jugador_size = 2 * sizeof(int16_t) + 80 * sizeof(char);
        alloc_data(jugador_size);
        char * tmp = _data;
        memcpy(tmp, name, 80 * sizeof(char));
        tmp += 80 * sizeof(char);
        memcpy(tmp, &x, sizeof(int16_t));
        tmp += sizeof(int16_t);
        memcpy(tmp, &y, sizeof(int16_t));
    }

    int from_bin(char * data)
    {
        memcpy(name, data, 80 * sizeof(char));
        data += 80 * sizeof(char);
        memcpy(&x, data, sizeof(int16_t));
        data += sizeof(int16_t);
        memcpy(&y, data, sizeof(int16_t));
        return 0;
    }


public:
    char name[80];

    int16_t x;
    int16_t y;
};

int main(int argc, char **argv)
{
    Jugador one_r("", 0, 0);
    Jugador one_w("Player_ONE", 123, 987);

    // 1. Serializar el objeto one_w
    one_w.to_bin();
    // 2. Escribir la serialización en un fichero
    int fd = open("./jugador.bin", O_CREAT | O_TRUNC | O_RDWR, 0666);
    write(fd, one_w.data(), one_w.size());
    close(fd);
    // 3. Leer el fichero
    char buffer[256];
    fd = open("./jugador.bin", O_RDONLY);
    read(fd, buffer, 256);
    // 4. "Deserializar" en one_r
    one_r.from_bin(buffer);
    // 5. Mostrar el contenido de one_r
    std::cout << "N: " << one_r.name << std::endl;
    std::cout << "x: " << one_r.x << std::endl;
    std::cout << "y: " << one_r.y << std::endl;

    return 0;
}

/*
Comando: od -sa jugador.bin
Nos permite convertir la entrada (jugador.bin) en diferentes formatos (por defecto en octal)
Los parámetros s y a nos muestran números en decimal y chars respectivamente
*/  
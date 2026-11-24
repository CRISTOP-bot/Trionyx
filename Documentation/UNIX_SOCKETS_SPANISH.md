# Sockets UNIX sencillos para Trionyx

API header-only para sockets locales `AF_UNIX` de tipo `SOCK_STREAM`. Usa la biblioteca estándar de C++ y las interfaces POSIX del sistema; no agrega dependencias externas. Está en `trionyx::ipc` y evita `std::array`.

## Conectar un cliente

```cpp
#include "trionyx/unix_socket.hpp"

int main() {
    trionyx::ipc::UnixStream socket;
    if (!socket.connect("/run/trionyx/service.sock")) {
        // Revisar errno para conocer la causa.
        return 1;
    }

    const char request[] = "hola";
    if (!trionyx::ipc::send_frame(socket, request, sizeof(request) - 1)) {
        return 1;
    }

    char response[256];
    std::uint32_t response_size = 0;
    if (!trionyx::ipc::receive_frame(socket, response, sizeof(response),
                                     response_size)) {
        return 1;
    }
    // response_size indica cuántos bytes son válidos; no se agrega '\0'.
}
```

## Atender conexiones

```cpp
#include "trionyx/unix_socket.hpp"
#include <cerrno>
#include <unistd.h>

int main() {
    const char* path = "/run/trionyx/service.sock";
    trionyx::ipc::UnixServer server;
    if (!server.listen(path)) {
        return 1; // errno contiene el error (por ejemplo, EADDRINUSE).
    }

    int accept_error = 0;
    trionyx::ipc::UnixStream client = server.accept(&accept_error);
    if (!client.valid()) {
        server.close();
        ::unlink(path); // UnixServer no elimina la ruta automáticamente.
        return 1;
    }

    char request[256];
    std::uint32_t request_size = 0;
    const bool ok = trionyx::ipc::receive_frame(
        client, request, sizeof(request), request_size);

    server.close();
    ::unlink(path); // Elimina solo la ruta que creó la aplicación.
    return ok ? 0 : 1;
}
```

## Protocolo de trama

Cada mensaje lleva un entero sin signo de 32 bits con el tamaño del payload, en orden de red (big-endian), seguido por el payload binario. Se puede enviar cualquier byte, incluidos ceros. `send_frame` y `receive_frame` se ocupan de lecturas/escrituras parciales e interrupciones `EINTR`.

`receive_frame` devuelve `EMSGSIZE` si la trama no cabe en el buffer. En ese caso el cuerpo no se consume: cierra o descarta la conexión en vez de intentar leer otra trama del mismo flujo. Las funciones devuelven `false` en error y dejan el código POSIX en `errno`; no lanzan excepciones. El descriptor RAII de `UnixStream` cierra la conexión al destruirse.

## Pruebas

Compilación y ejecución directa en Linux/POSIX, sin Meson ni bibliotecas externas adicionales:

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/unix_socket_test.cpp -o /tmp/unix_socket_test
/tmp/unix_socket_test
```

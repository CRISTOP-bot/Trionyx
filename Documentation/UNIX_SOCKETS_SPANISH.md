# Sockets UNIX sencillos para Trionyx

API header-only para sockets locales POSIX `AF_UNIX` de tipo `SOCK_STREAM`. No agrega dependencias externas y evita `std::array`. Está en `trionyx::ipc`.

## Cliente con `std::string`

```cpp
#include "trionyx/unix_socket.hpp"
#include <string>

int main() {
    trionyx::ipc::UnixStream socket;
    if (!socket.connect(std::string("/run/trionyx/service.sock"))) {
        return 1; // Revisar errno.
    }

    const std::string request = "hola";
    if (!trionyx::ipc::send_frame(socket, request)) {
        return 1;
    }

    std::string response;
    if (!trionyx::ipc::receive_frame(socket, response)) {
        return 1;
    }
    // response.size() da el tamaño; puede contener NUL y no agrega terminador.
}
```

`send_frame` acepta `std::string`/`std::string_view` y conserva payloads binarios. También se puede recibir en un buffer fijo si se quiere evitar asignación dinámica:

```cpp
char response[256];
std::uint32_t size = 0;
if (!trionyx::ipc::receive_frame(socket, response, sizeof(response), size)) {
    // Revisar errno.
}
```

## Servidor

```cpp
#include "trionyx/unix_socket.hpp"
#include <string>
#include <unistd.h>

int main() {
    const std::string path = "/run/trionyx/service.sock";
    trionyx::ipc::UnixServer server;
    if (!server.listen(path)) {
        return 1; // errno contiene el error.
    }

    trionyx::ipc::UnixStream client = server.accept();
    if (!client.valid()) {
        server.close();
        ::unlink(path.c_str()); // UnixServer no elimina la ruta automáticamente.
        return 1;
    }

    std::string request;
    const bool ok = trionyx::ipc::receive_frame(client, request);
    server.close();
    ::unlink(path.c_str());
    return ok ? 0 : 1;
}
```

`UnixStream::connect` y `UnixServer::listen` admiten `const char*` y `std::string`. Rutas con bytes NUL incrustados se rechazan con `EINVAL`.

## Protocolo y límites

Cada trama contiene un entero sin signo de 32 bits con el tamaño del payload, en orden de red (big-endian), seguido por el payload binario. Las operaciones manejan lecturas/escrituras parciales e interrupciones `EINTR`.

La sobrecarga `receive_frame(stream, string)` limita el payload a 16 MiB por defecto (`default_max_frame_size`). Se puede cambiar con `receive_frame(stream, string, max_size)`. Si se excede el límite, devuelve `false` y fija `errno` a `EMSGSIZE`; el cuerpo no se consume, así que descarta/cierra esa conexión. El buffer de string crece dinámicamente y una falla de asignación puede propagar una excepción estándar. La sobrecarga de buffer fijo no realiza esa asignación.

Los errores de socket devuelven `false` y se consultan en `errno`; las operaciones de `UnixStream` y `UnixServer` no lanzan excepciones. `UnixStream` cierra su descriptor al destruirse. El servidor no desvincula automáticamente la ruta del socket.

## Pruebas y CI

Compilación y ejecución en Linux/POSIX, sin dependencias externas adicionales:

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/unix_socket_test.cpp -o /tmp/unix_socket_test
/tmp/unix_socket_test
```

GitHub Actions ejecuta el mismo conjunto de pruebas con GCC y Clang en cada push a `dev` o `main` y en pull requests hacia esas ramas. El workflow está en `.github/workflows/ci.yml`.

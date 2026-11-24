# Trionyx

Trionyx es una distribución GNU/Linux x86_64 en desarrollo. Este repositorio también contiene componentes de sistema y utilidades de bajo nivel en C/C++.

## API sencilla de sockets UNIX

La API header-only está en `include/trionyx/unix_socket.hpp`, dentro del espacio de nombres `trionyx::ipc`. Permite abrir conexiones locales `AF_UNIX` de tipo `SOCK_STREAM` sin bibliotecas externas ni `std::array`.

Incluye:

- `UnixStream`: conexión cliente, envío y recepción exactos y propiedad RAII del descriptor.
- `UnixServer`: enlazar y escuchar en una ruta de socket, y aceptar clientes.
- `send_frame` / `receive_frame`: protocolo binario con longitud de 32 bits en orden de red seguida por el payload.
- Reintentos ante `EINTR`; los errores se comunican con `false` y `errno`, sin excepciones.

### Ejemplo de cliente

```cpp
#include "trionyx/unix_socket.hpp"
#include <cstdint>

int main() {
    trionyx::ipc::UnixStream socket;
    if (!socket.connect("/run/trionyx/service.sock")) {
        return 1; // Consultar errno.
    }

    const char request[] = "hola";
    if (!trionyx::ipc::send_frame(socket, request, sizeof(request) - 1)) {
        return 1;
    }

    char response[256];
    std::uint32_t size = 0;
    if (!trionyx::ipc::receive_frame(socket, response, sizeof(response), size)) {
        return 1;
    }
    // `size` es la cantidad de bytes válidos; el payload no termina en NUL.
    return 0;
}
```

### Compilar y ejecutar las pruebas

En Linux con un compilador C++23 y pthreads:

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/unix_socket_test.cpp -o /tmp/trionyx-unix-socket-test
/tmp/trionyx-unix-socket-test
```

Las pruebas cubren payload binario (incluidos bytes NUL), tramas vacías, rechazo de tramas que exceden el buffer y una ida y vuelta real entre servidor y cliente.

### CI

GitHub Actions compila y ejecuta estas pruebas en cada push a `dev` o `main` y en pull requests hacia esas ramas, usando GCC y Clang. El workflow está en `.github/workflows/ci.yml`.

### Documentación ampliada

Consulta [`Documentation/UNIX_SOCKETS_SPANISH.md`](Documentation/UNIX_SOCKETS_SPANISH.md) para ver el protocolo, ejemplos de servidor y cliente, y el comportamiento ante errores.

## Estado del proyecto

La API de sockets UNIX es una utilidad pequeña e independiente dentro del repositorio. No implica que la distribución o todos sus componentes estén completos o validados como sistema arrancable.

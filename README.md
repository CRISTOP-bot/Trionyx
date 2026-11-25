# Trionyx

Trionyx es una distribución GNU/Linux x86_64 en desarrollo. Este repositorio también contiene componentes de sistema y utilidades de bajo nivel en C/C++.

## API sencilla de sockets UNIX

La API header-only está en `include/trionyx/unix_socket.hpp`, dentro del espacio de nombres `trionyx::ipc`. Usa sockets locales POSIX `AF_UNIX` de tipo `SOCK_STREAM`, sin dependencias externas ni `std::array`.

### Uso rápido con `std::string`

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
    // response contiene el payload y puede incluir bytes NUL.
    return 0;
}
```

- `UnixStream::connect` y `UnixServer::listen` aceptan `const char*` o `std::string`.
- `send_frame` acepta `std::string`, `std::string_view` y literales; conserva los bytes NUL de strings.
- `receive_frame` puede escribir directo en un `std::string`; por defecto limita cada trama a 16 MiB. Pasa un límite explícito si necesitas otro tamaño.
- La sobrecarga de buffer (`void*`, capacidad y tamaño recibido) sigue disponible para evitar asignaciones dinámicas.
- `UnixStream` posee el descriptor y lo cierra automáticamente. Las funciones POSIX fallidas devuelven `false` y dejan el error en `errno`; el crecimiento del string recibido puede lanzar una excepción de asignación.

El protocolo de trama tiene un tamaño de 32 bits en orden de red, seguido por los bytes del payload. La referencia completa está en [`Documentation/UNIX_SOCKETS_SPANISH.md`](Documentation/UNIX_SOCKETS_SPANISH.md).

El servicio `sysdata` de init añade `GET`/`SET` y `GETP`/`SETP` por `/dev/socket/sysdata`, con claves validadas (sin espacios, ni `.` al inicio/final), identidad de proceso validada por PID y tiempo de inicio, y una proyección best-effort de consulta en `/dev/reg` que nunca condiciona la tabla autoritativa. Consulta [`Documentation/SYSDATA_SPANISH.md`](Documentation/SYSDATA_SPANISH.md); el punto de entrada de init debe invocar `run_init_service()` para activarlo.

El helper de salida `stdr::out` de `include/types.h` también acepta rutas y contenido `std::string`; las rutas con NUL incrustado se rechazan con `EINVAL`.

### Compilar y ejecutar las pruebas

En Linux con un compilador C++23 y pthreads:

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/unix_socket_test.cpp -o /tmp/trionyx-unix-socket-test
/tmp/trionyx-unix-socket-test
```

Las pruebas cubren tramas binarias, NUL dentro de `std::string`, el límite de tamaño, una comunicación cliente-servidor, la salida a archivo de `stdr::out` con una ruta `std::string` y el protocolo `sysdata` (permisos, claves y limpieza por PID).

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/sysdata_test.cpp -o /tmp/trionyx-sysdata-test
/tmp/trionyx-sysdata-test
```

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -Iinclude tests/types_out_test.cpp src/builtin.cpp \
  -o /tmp/trionyx-types-out-test
/tmp/trionyx-types-out-test
```

### CI

GitHub Actions compila y ejecuta las pruebas en cada push a `dev` o `main` y en pull requests hacia esas ramas, usando GCC y Clang. El workflow está en `.github/workflows/ci.yml`.

## Estado del proyecto

La API de sockets UNIX es una utilidad pequeña e independiente dentro del repositorio. No implica que la distribución o todos sus componentes estén completos o validados como sistema arrancable.

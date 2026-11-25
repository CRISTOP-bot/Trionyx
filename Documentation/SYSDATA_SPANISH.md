# Servicio sysdata de Trionyx

`include/trionyx/sysdata.hpp` implementa el servicio de propiedades del proceso init. Hay exactamente dos objetos globales de almacenamiento: `normal_store` (clave/valor) y `private_store` (PID, clave y valor, más metadato de tiempo de inicio para evitar reutilización de PID). Los objetos `Store` locales son vistas de init o backends aislados de pruebas; no crean otro estado global. Los clientes solo hablan con init por el socket local.

> La rama actual no tiene un punto de entrada activo de init donde conectar automáticamente este servicio. El entrypoint de init debe invocar `trionyx::sysdata::run_init_service()` después de preparar `/dev`. El montaje de `/dev/reg` es best-effort y no bloquea el servicio de socket ni las operaciones de propiedades.

## Protocolo de texto por línea

Cada petición termina en `\n`; cada respuesta también. Un cliente puede enviar varias peticiones en la misma conexión.

```text
GET <key>\n
SET <key> <value>\n
GETP <key>\n
SETP <key> <value>\n
```

Respuestas:

```text
OK\n                 # SET / SETP
OK <value>\n        # GET / GETP
ERR NOT_FOUND\n
ERR PERMISSION_DENIED\n
ERR INVALID\n
ERR IO\n```

Valores vacíos, NUL, CR y LF se rechazan. Las claves no pueden estar vacías ni contener espacios (ni tabuladores), y solo admiten letras ASCII, dígitos, `_`, `-` y `.`. No pueden empezar o terminar en `.`; también se rechaza `..`. El formato de línea permite espacios dentro del valor; para representar una línea o NUL se tendría que definir otro protocolo/escape, por lo que esos bytes se rechazan.

## `GET` y `SET`

Las propiedades normales están en el vector de init y sobreviven a la salida del PID que hizo `SET` mientras init siga activo. El almacenamiento en memoria no promete persistencia entre reinicios.

`ro.*` e `init.*` solo pueden cambiarse mediante una petición autenticada cuyo PID sea el PID de init. La API local de init puede establecerlas directamente:

```cpp
trionyx::sysdata::setkey("ro.product", "Trionyx");
trionyx::sysdata::setkey("init.ready", "yes");
```

`GET` permite consultar cualquier clave válida.

## `GETP` y `SETP`

El registro privado conserva los campos `(pid_t, key, value)` y el tiempo de inicio del proceso leído de `/proc/<pid>/stat` como metadato de identidad. El PID se obtiene del kernel usando las credenciales del socket UNIX (`SO_PEERCRED` en Linux); nunca se acepta un PID del texto enviado por el cliente. La identidad se compara por PID y tiempo de inicio, así que un proceso nuevo que reutilice el mismo número de PID no hereda las propiedades del anterior.

- El primer `SETP` de una clave la asocia al PID solicitante.
- Solo ese PID puede volver a escribirla o leerla. Otro PID recibe `ERR PERMISSION_DENIED`; init (PID 1) puede leer y modificar cualquier entrada privada.
- Al morir el proceso propietario, sus entradas se eliminan en el mantenimiento periódico o en la siguiente operación. Además, cada acceso valida el tiempo de inicio; no depende de que el mantenimiento corra de inmediato y la reutilización del PID no permite recuperar valores viejos.
- El namespace `*P` solo usa el vector de proceso: no escribe ni lee `/dev/reg` y no aparece en el vector normal.

## Proyección de registro `/dev/reg`

Al iniciar el servicio, `run_init_service()` intenta preparar `/dev/reg` como tmpfs y reproyecta las claves existentes. Para cada `SET` y cada `setkey` exitoso intenta escribir una proyección de solo consulta:

```text
SET sys.boot.completed cebolla
```

se refleja en:

```text
/dev/reg/sys/boot/completed
```

`GET` siempre consulta el vector en memoria, nunca el archivo. Editar, borrar o falsificar un archivo de `/dev/reg` no cambia el vector ni el resultado de `GET`. `SETP`/`GETP` no se reflejan allí. El archivo es una proyección/diagnóstico, no almacenamiento de confianza. Que el montaje, directorio o archivo no exista o no se pueda escribir no rechaza ni revierte `SET`/`setkey`: el vector autoritativo se actualiza y la proyección se considera best-effort. La vista puede quedar incompleta si falla el filesystem.

El servidor crea `/dev/socket/sysdata` con permisos de conexión local; la autorización se aplica por PID peer en cada conexión. `run_init_service()` crea un pthread detached y retorna para que init continúe el arranque. El hilo atiende una conexión a la vez. Los dos vectores globales tienen mutexes pthread separados; las vistas `Store` pueden usarse concurrentemente desde el hilo de init y el servicio.

## Pruebas

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/sysdata_test.cpp -o /tmp/trionyx-sysdata-test
/tmp/trionyx-sysdata-test
```

Las pruebas usan directorios temporales para la proyección; no montan ni modifican `/dev`. Cubren límites de claves/valores, namespaces protegidos, manipulación y fallo de la proyección sin bloquear actualizaciones, persistencia normal tras salir un PID, identidad de proceso ligada al tiempo de inicio, acceso administrativo de init a todas las claves privadas, concurrencia entre varios hilos, limpieza del namespace de proceso y autenticación de las credenciales del peer. El CI ejecuta GCC y Clang.

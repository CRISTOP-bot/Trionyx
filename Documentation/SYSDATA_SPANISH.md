# Servicio sysdata de Trionyx

`include/trionyx/sysdata.hpp` implementa el servicio de propiedades del proceso init. El estado autoritativo vive en dos `std::vector` dentro de `trionyx::sysdata::Store`; los clientes solo hablan con init por el socket local.

> La rama actual no tiene un punto de entrada activo de init donde conectar automáticamente este servicio. El entrypoint de init debe invocar `trionyx::sysdata::run_init_service()` después de preparar `/dev`. Ese método monta/valida el tmpfs de registro y atiende `/dev/socket/sysdata` en bucle.

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

El vector de proceso almacena `key`, `value` y `pid`. El PID se obtiene del kernel usando las credenciales del socket UNIX (`SO_PEERCRED` en Linux); nunca se acepta un PID del texto enviado por el cliente.

- El primer `SETP` de una clave la asocia al PID solicitante.
- Solo ese PID puede volver a escribirla o leerla. Otro PID recibe `ERR PERMISSION_DENIED`.
- Al morir el PID propietario, init elimina sus entradas de proceso durante el mantenimiento periódico (máximo alrededor de un segundo) o en la siguiente petición.
- El namespace `*P` solo usa el vector de proceso: no escribe ni lee `/dev/reg` y no aparece en el vector normal.

## Proyección de registro `/dev/reg`

Al iniciar el servicio, `run_init_service()` prepara `/dev/reg` como tmpfs. Para cada `SET` y cada `setkey` exitoso se escribe una proyección de solo consulta:

```text
SET sys.boot.completed cebolla
```

se refleja en:

```text
/dev/reg/sys/boot/completed
```

`GET` siempre consulta el vector en memoria, nunca el archivo. Editar, borrar o falsificar un archivo de `/dev/reg` no cambia el vector ni el resultado de `GET`. `SETP`/`GETP` no se reflejan allí. El archivo es una proyección/diagnóstico, no almacenamiento de confianza.

El servidor crea `/dev/socket/sysdata` con permisos de conexión local; la autorización se aplica por PID peer en cada conexión. `Store` atiende peticiones secuencialmente y está diseñado para ser propiedad de init.

## Pruebas

```sh
c++ -std=c++23 -Wall -Wextra -Werror -pedantic -pthread \
  -Iinclude tests/sysdata_test.cpp -o /tmp/trionyx-sysdata-test
/tmp/trionyx-sysdata-test
```

Las pruebas usan un directorio temporal para la proyección; no montan ni modifican `/dev`. Cubren límites de claves/valores, namespaces protegidos, manipulación de archivos de registro, persistencia normal tras salir un PID, limpieza del namespace de proceso y autenticación de las credenciales del peer. El CI ejecuta GCC y Clang.

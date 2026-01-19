cmake_minimum_required(VERSION 3.10)

project(communicator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Ostrzeżenia – bardzo polecane
add_compile_options(-Wall -Wextra -Wpedantic)

# Katalog z nagłówkami (dzięki temu #include "tcp_server.h" działa)
include_directories(include)

# --- 1. BIBLIOTEKA WSPÓLNA (Protokół) ---
# Kompilujemy protocol.c jako bibliotekę, bo używa jej i klient, i serwer.
add_library(protocol
    src/protocol.c
)

# --- 2. SERWER ---
# Dodajemy wszystkie pliki .c składające się na serwer.
# UWAGA: Upewnij się, że funkcja 'get_local_ip' jest w którymś z tych plików!
add_executable(server
    src/server.c
    src/tcp_server.c
    src/multicast_server.c
)

# Linkujemy bibliotekę protokołu oraz wątki (pthread)
target_link_libraries(server
    protocol
    pthread
)

# --- 3. KLIENT ---
add_executable(client
    src/client.c
)

target_link_libraries(client
    protocol
    pthread
)

# --- 4. KLIENT DEBUG (do testów) ---
add_executable(client_debug
    src/client_debug.c
)

target_link_libraries(client_debug
    protocol
    pthread
)
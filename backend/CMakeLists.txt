cmake_minimum_required(VERSION 3.10)
project(MIA_Backend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar archivos .cpp en src y todas sus subcarpetas
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(mia_server ${SOURCES})

# Librerías necesarias
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(mia_server PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    ZLIB::ZLIB
    pthread
)

target_include_directories(mia_server PRIVATE src)
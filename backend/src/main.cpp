#include "httplib.h"
#include "structs.h"

// Discos
#include "comandos/disco/mkdisk.h"
#include "comandos/disco/rmdisk.h"

// Particiones
#include "comandos/particion/fdisk.h"
#include "comandos/particion/mount.h"
#include "comandos/particion/mounted.h"
#include "comandos/particion/unmount.h"

// Filesystem
#include "comandos/filesystem/mkfs.h"
#include "comandos/filesystem/mkdir.h"
#include "comandos/filesystem/touch.h"
#include "comandos/filesystem/cat.h"

// Usuarios
#include "comandos/usuarios/login.h"
#include "comandos/usuarios/logout.h"

// Reportes
#include "comandos/reportes/rep.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>

using namespace std;

// Función auxiliar para headers CORS
void setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// Handler genérico para manejar CORS preflight en todas las rutas
bool corsHandler(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);
    
    // Si es OPTIONS (preflight), responder 200 sin cuerpo
    if (req.method == "OPTIONS") {
        res.status = 200;
        res.set_content("", "text/plain");
        return true;  // Request handled
    }
    return false;  // Let other handlers process
}

string ejecutarComando(string comando) {
    istringstream iss(comando);
    string cmd;
    iss >> cmd;
    for (char& c : cmd) c = tolower(c);
    
    if (cmd == "mkdisk") return MKDisk::ejecutar(comando);
    else if (cmd == "rmdisk") return RMDisk::ejecutar(comando);
    else if (cmd == "fdisk") return FDisk::ejecutar(comando);
    else if (cmd == "mount") return Mount::ejecutar(comando);
    else if (cmd == "mounted") return Mounted::ejecutar(comando);
    else if (cmd == "unmount") return Unmount::ejecutar(comando);
    else if (cmd == "mkfs") return MKFS::ejecutar(comando);
    else if (cmd == "mkdir") return MkDir::ejecutar(comando);
    else if (cmd == "touch") return Touch::ejecutar(comando);
    else if (cmd == "cat") return Cat::ejecutar(comando);
    else if (cmd == "login") return Login::ejecutar(comando);
    else if (cmd == "logout") return Logout::ejecutar(comando);
    else if (cmd == "rep") return Rep::ejecutar(comando);
    
    return "Error: Comando no reconocido: " + cmd;
}

int main() {
    httplib::Server svr;

    // ✅ CORS: Usar set_pre_routing_handler (compatible con cpp-httplib)
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        
        // Manejar preflight OPTIONS
        if (req.method == "OPTIONS") {
            res.status = 200;
            res.set_content("", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Endpoint POST /api/comando
    svr.Post("/api/comando", [](const httplib::Request &req, httplib::Response &res) {
        if (req.has_param("comando")) {
            string comando = req.get_param_value("comando");
            string resultado = ejecutarComando(comando);
            res.set_content(resultado, "text/plain");
        } else {
            res.set_content("Error: No se recibió comando", "text/plain");
        }
    });

    // Endpoint GET para pruebas
    svr.Get("/api/test", [](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Backend OK", "text/plain");
    });

    cout << "========================================" << endl;
    cout << "  Servidor C++ ExtreamFS iniciado" << endl;
    cout << "  URL: http://localhost:8080" << endl;
    cout << "  Carnet: 202404730 (IDs: 301A, 301B...)" << endl;
    cout << "  Presiona Ctrl+C para detener" << endl;
    cout << "========================================" << endl;
    cout.flush();

    // Escuchar en localhost para desarrollo local
    if (!svr.listen("127.0.0.1", 8080)) {
        cerr << "Error: No se pudo iniciar el servidor en puerto 8080" << endl;
        return 1;
    }
    
    return 0;
}
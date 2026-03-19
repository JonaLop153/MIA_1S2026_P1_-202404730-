#include "httplib.h"
#include "structs.h"
#include "comandos/disco/mkdisk.h"
#include "comandos/disco/rmdisk.h"
#include "comandos/particion/fdisk.h"
#include "comandos/particion/mount.h"
#include "comandos/particion/mounted.h"
#include "comandos/particion/unmount.h"
#include "comandos/filesystem/mkfs.h"
#include "comandos/filesystem/mkdir.h"
#include "comandos/filesystem/mkfile.h"
#include "comandos/filesystem/touch.h"
#include "comandos/filesystem/cat.h"
#include "comandos/usuarios/login.h"
#include "comandos/usuarios/logout.h"
#include "comandos/usuarios/mkgrp.h"
#include "comandos/usuarios/rmgrp.h"
#include "comandos/usuarios/mkusr.h"
#include "comandos/usuarios/rmusr.h"
#include "comandos/usuarios/chgrp.h"
#include "comandos/reportes/rep.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>
using namespace std;

string ejecutarComando(string comando) {
    istringstream iss(comando);
    string cmd;
    iss >> cmd;
    // ✅ Convertir comando a minúsculas (case insensitive)
    for (char& c : cmd) c = tolower(c);
    
    if (cmd == "mkdisk") return MKDisk::ejecutar(comando);
    else if (cmd == "rmdisk") return RMDisk::ejecutar(comando);
    else if (cmd == "fdisk") return FDisk::ejecutar(comando);
    else if (cmd == "mount") return Mount::ejecutar(comando);
    else if (cmd == "mounted") return Mounted::ejecutar(comando);
    else if (cmd == "unmount") return Unmount::ejecutar(comando);
    else if (cmd == "mkfs") return MKFS::ejecutar(comando);
    else if (cmd == "mkdir") return MkDir::ejecutar(comando);
    else if (cmd == "mkfile") return MkFile::ejecutar(comando);
    else if (cmd == "touch") return Touch::ejecutar(comando);
    else if (cmd == "cat") return Cat::ejecutar(comando);
    else if (cmd == "login") return Login::ejecutar(comando);
    else if (cmd == "logout") return Logout::ejecutar(comando);
    else if (cmd == "mkgrp") return MkGrp::ejecutar(comando);
    else if (cmd == "rmgrp") return RmGrp::ejecutar(comando);
    else if (cmd == "mkusr") return MkUsr::ejecutar(comando);
    else if (cmd == "rmusr") return RmUsr::ejecutar(comando);
    else if (cmd == "chgrp") return ChGrp::ejecutar(comando);
    else if (cmd == "rep") return Rep::ejecutar(comando);
    return "Error: Comando no reconocido: " + cmd;
}

void setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

int main() {
    httplib::Server svr;
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res);
        if (req.method == "OPTIONS") {
            res.status = 200;
            res.set_content("", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Post("/api/comando", [](const httplib::Request &req, httplib::Response &res) {
        if (req.has_param("comando")) {
            string comando = req.get_param_value("comando");
            string resultado = ejecutarComando(comando);
            res.set_content(resultado, "text/plain");
        } else {
            res.set_content("Error: No se recibió comando", "text/plain");
        }
    });

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

    if (!svr.listen("127.0.0.1", 8080)) {
        cerr << "Error: No se pudo iniciar el servidor" << endl;
        return 1;
    }
    return 0;
}
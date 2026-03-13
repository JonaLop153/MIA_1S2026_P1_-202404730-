#include "login.h"
#include "../../session/session.h"
#include "../../structs.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

map<string, string> Login::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;
    
    while (iss >> token) {
        if (token[0] == '-') {
            size_t eqPos = token.find('=');
            if (eqPos != string::npos) {
                string key = token.substr(1, eqPos - 1);
                string value = token.substr(eqPos + 1);
                
                if (!value.empty() && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                for (char& c : key) c = tolower(c);
                params[key] = value;
            }
        }
    }
    return params;
}

bool Login::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("user") == params.end()) {
        error = "Error: El parámetro -user es obligatorio";
        return false;
    }
    if (params.find("pass") == params.end()) {
        error = "Error: El parámetro -pass es obligatorio";
        return false;
    }
    if (params.find("id") == params.end()) {
        error = "Error: El parámetro -id es obligatorio";
        return false;
    }
    return true;
}

bool Login::autenticarUsuario(const string& id, const string& user, const string& pass) {
    // Buscar la partición montada
    MountMap& montadas = getParticionesMontadas();
    
    if (montadas.find(id) == montadas.end()) {
        return false;
    }
    
    string path = montadas.at(id).first;
    
    // Leer users.txt (en implementación completa, leer del sistema EXT2)
    // Por ahora, validación simplificada para root
    if (user == "root" && pass == "123") {
        return true;
    }
    
    return false;
}

string Login::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    // Verificar si ya hay sesión activa
    if (haySesionActiva()) {
        return "Error: Ya hay una sesión activa. Debe hacer logout primero";
    }
    
    string user = params["user"];
    string pass = params["pass"];
    string id = params["id"];
    
    if (!autenticarUsuario(id, user, pass)) {
        return "Error: Autenticación fallida. Usuario o contraseña incorrectos";
    }
    
    // Iniciar sesión
    iniciarSesion(user, pass, id);
    
    ostringstream oss;
    oss << "Sesión iniciada exitosamente:\n";
    oss << "  Usuario: " << user << "\n";
    oss << "  ID Partición: " << id;
    
    return oss.str();
}
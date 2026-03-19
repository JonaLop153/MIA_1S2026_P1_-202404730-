#include "rmdisk.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

map<string, string> RMDisk::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "rmdisk"
    
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

bool RMDisk::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    return true;
}

string RMDisk::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string path = params["path"];
    
    // ✅ VERIFICAR QUE EL ARCHIVO EXISTE ANTES DE ELIMINAR
    if (!fs::exists(path)) {
        return "Error: El disco no existe en: " + path;
    }
    
    // Intentar eliminar el archivo
    try {
        fs::remove(path);
    } catch (const fs::filesystem_error& e) {
        return "Error: No se pudo eliminar el disco: " + string(e.what());
    }
    
    ostringstream oss;
    oss << "Disco eliminado exitosamente: " << path;
    
    return oss.str();
}
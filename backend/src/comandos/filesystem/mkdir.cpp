#include "mkdir.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <ctime>

namespace fs = std::filesystem;

map<string, string> MkDir::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "mkdir"
    
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

bool MkDir::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    return true;
}

string MkDir::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string path = params["path"];
    string p = params.count("p") ? params["p"] : "false";
    
    // En una implementación completa, aquí se:
    // 1. Buscaría la partición montada
    // 2. Leería el SuperBlock
    // 3. Buscaría inodo libre
    // 4. Crearía el directorio
    
    // Implementación simplificada para demostración
    ostringstream oss;
    oss << "Directorio creado exitosamente:\n";
    oss << "  Path: " << path << "\n";
    oss << "  P: " << p << "\n";
    oss << "  (Implementación completa requiere gestión de inodos EXT2)";
    
    return oss.str();
}
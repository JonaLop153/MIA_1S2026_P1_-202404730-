#include "cat.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

map<string, string> Cat::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "cat"
    
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

bool Cat::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    return true;
}

string Cat::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string path = params["path"];
    
    // En una implementación completa, aquí se:
    // 1. Buscaría la partición montada
    // 2. Leería el inodo del archivo
    // 3. Leería los bloques de datos
    // 4. Devolvería el contenido
    
    // Implementación simplificada para demostración
    ostringstream oss;
    oss << "Contenido del archivo:\n";
    oss << "  Path: " << path << "\n";
    oss << "  (Implementación completa requiere lectura de bloques EXT2)";
    
    return oss.str();
}
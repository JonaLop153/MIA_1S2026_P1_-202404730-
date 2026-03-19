#include "cat.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

map<string, string> Cat::parsearParametros(const string& comando) {
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
                if (!value.empty() && value.front() == '"' && value.back() == '"')
                    value = value.substr(1, value.length() - 2);
                for (char& c : key) c = tolower(c);
                params[key] = value;
            }
        }
    }
    return params;
}

bool Cat::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("file1") == params.end()) { error = "Error: -file1 obligatorio"; return false; }
    return true;
}

string Cat::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa";
    
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    string filePath = params["file1"];
    string id = getIdParticionActual();
    
    MountMap& montadas = getParticionesMontadas();
    if (montadas.find(id) == montadas.end()) return "Error: Partición no montada";
    
    string diskPath = montadas.at(id).first;
    int indice = montadas.at(id).second;
    
    // ✅ Abrir archivo del disco
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    // ✅ Leer MBR
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    int partStart = mbr.mbr_partitions[indice].part_start;
    
    // ✅ Leer SuperBlock
    file.seekg(partStart, ios::beg);
    SuperBlock sb;
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    
    // ✅ CASO ESPECIAL: /users.txt (inodo 3)
    if (filePath == "/users.txt") {
        // Leer inodo 3
        file.seekg(partStart + sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
        Inodo inodoUsers;
        file.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo));
        
        // Leer bloque de users.txt
        int bloqueNum = inodoUsers.i_block[0];
        int bloqueOffset = partStart + sb.s_block_start + (bloqueNum * sb.s_block_s);
        
        file.seekg(bloqueOffset, ios::beg);
        char buffer[64];
        memset(buffer, 0, 64);
        file.read(buffer, 64);
        file.close();
        
        // ✅ Retornar contenido legible
        string contenido = "";
        for (int i = 0; i < 64; i++) {
            if (buffer[i] == '\0') break;
            if (buffer[i] == '\n' || (buffer[i] >= 32 && buffer[i] <= 126)) {
                contenido += buffer[i];
            }
        }
        
        // DEBUG en terminal del backend
        std::cerr << "DEBUG CAT: users.txt contenido='" << contenido << "'" << std::endl;
        
        return contenido;
    }
    
    file.close();
    
    // ✅ Otros archivos (implementación simplificada)
    ostringstream oss;
    oss << "Contenido del archivo: " << filePath << "\n";
    oss << "----------------------------------------\n";
    oss << "(Implementación completa requiere búsqueda de inodos por path)\n";
    oss << "----------------------------------------\n";
    return oss.str();
}
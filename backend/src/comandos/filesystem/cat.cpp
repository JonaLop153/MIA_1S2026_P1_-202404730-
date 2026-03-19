#include "cat.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

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
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    int partStart = mbr.mbr_partitions[indice].part_start;
    
    ifstream sbFile(diskPath, ios::binary | ios::in);
    sbFile.seekg(partStart, ios::beg);
    SuperBlock sb;
    sbFile.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    sbFile.close();
    
    // ✅ Caso especial: /users.txt (inodo 3)
    if (filePath == "/users.txt") {
        // Leer inodo 3
        ifstream inodeFile(diskPath, ios::binary | ios::in);
        inodeFile.seekg(sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
        Inodo inodoUsers;
        inodeFile.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo));
        inodeFile.close();
        
        // Leer bloque de users.txt
        int bloqueUsers = inodoUsers.i_block[0];
        ifstream blockFile(diskPath, ios::binary | ios::in);
        blockFile.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
        BloqueArchivo bloqueContent;
        blockFile.read(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
        blockFile.close();
        
        // ✅ Mostrar contenido completo (hasta 64 bytes)
        string contenido = "";
        for (int i = 0; i < 64 && bloqueContent.b_content[i] != '\0'; i++) {
            char c = bloqueContent.b_content[i];
            if (c >= 32 && c <= 126) {
                contenido += c;
            } else if (c == '\n') {
                contenido += '\n';
            }
        }
        return contenido;
    }
    
    // Otros archivos (implementación simplificada)
    ostringstream oss;
    oss << "Contenido del archivo: " << filePath << "\n";
    oss << "----------------------------------------\n";
    oss << "(Implementación completa requiere búsqueda de inodos por path)\n";
    oss << "----------------------------------------\n";
    return oss.str();
}
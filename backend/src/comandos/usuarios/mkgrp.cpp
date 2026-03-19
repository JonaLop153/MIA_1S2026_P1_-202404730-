#include "mkgrp.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

map<string, string> MkGrp::parsearParametros(const string& comando) {
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

bool MkGrp::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("name") == params.end()) { error = "Error: -name obligatorio"; return false; }
    return true;
}

string MkGrp::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa. Inicie sesión primero";
    if (getUsuarioActual() != "root") return "Error: Solo root puede crear grupos";
    
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    string nombreGrupo = params["name"];
    string id = getIdParticionActual();
    MountMap& montadas = getParticionesMontadas();
    if (montadas.find(id) == montadas.end()) return "Error: Partición no montada";
    
    string diskPath = montadas.at(id).first;
    int indice = montadas.at(id).second;
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    MBR mbr; file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR)); file.close();
    int partStart = mbr.mbr_partitions[indice].part_start;
    
    ifstream sbFile(diskPath, ios::binary | ios::in);
    sbFile.seekg(partStart, ios::beg);
    SuperBlock sb; sbFile.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock)); sbFile.close();
    
    // Leer inodo 3 (users.txt)
    ifstream inodeFile(diskPath, ios::binary | ios::in);
    inodeFile.seekg(sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
    Inodo inodoUsers; inodeFile.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo)); inodeFile.close();
    
    int bloqueUsers = inodoUsers.i_block[0];
    ifstream blockFile(diskPath, ios::binary | ios::in);
    blockFile.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    BloqueArchivo bloqueContent; blockFile.read(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo)); blockFile.close();
    
    string contenido(bloqueContent.b_content);
    
    // ✅ Validar que el grupo no exista
    istringstream iss(contenido);
    string linea;
    while (getline(iss, linea)) {
        linea.erase(0, linea.find_first_not_of(" \t\r\n"));
        linea.erase(linea.find_last_not_of(" \t\r\n") + 1);
        if (linea.empty()) continue;
        
        istringstream lineStream(linea);
        string gidStr, type, grupo;
        getline(lineStream, gidStr, ',');
        getline(lineStream, type, ',');
        
        if (type.find('G') != string::npos) {
            getline(lineStream, grupo, ',');
            grupo.erase(0, grupo.find_first_not_of(" \t"));
            grupo.erase(grupo.find_last_not_of(" \t\r\n") + 1);
            
            if (grupo == nombreGrupo) {
                return "Error: El grupo ya existe: " + nombreGrupo;
            }
        }
    }
    
    // Calcular nuevo GID
   // Calcular nuevo GID (buscar el máximo existente)
int nuevoGID = 2;
istringstream maxIss(contenido);
string maxLinea;
while (getline(maxIss, maxLinea)) {
    maxLinea.erase(0, maxLinea.find_first_not_of(" \t\r\n"));
    maxLinea.erase(maxLinea.find_last_not_of(" \t\r\n") + 1);
    if (maxLinea.empty()) continue;
    
    istringstream maxStream(maxLinea);
    string gidStr, type;
    getline(maxStream, gidStr, ',');
    getline(maxStream, type, ',');
    
    if (type.find('G') != string::npos) {
        try {
            int gid = stoi(gidStr);
            if (gid >= nuevoGID) nuevoGID = gid + 1;
        } catch (...) {}
    }
}

// ✅ Agregar nueva línea al final
string nuevaLinea = to_string(nuevoGID) + ",G," + nombreGrupo + "\n";
string nuevoContenido = contenido;

// Si el contenido no termina con \n, agregarlo
if (!nuevoContenido.empty() && nuevoContenido.back() != '\n') {
    nuevoContenido += "\n";
}
nuevoContenido += nuevaLinea;

// ✅ Escribir de vuelta (máximo 63 bytes)
memset(bloqueContent.b_content, 0, 64);
size_t copySize = min(nuevoContenido.size(), (size_t)63);
strncpy(bloqueContent.b_content, nuevoContenido.c_str(), copySize);
    
    fstream writeBlock(diskPath, ios::binary | ios::in | ios::out);
    writeBlock.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    writeBlock.write(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
    writeBlock.close();
    
    return "Grupo creado exitosamente: " + nombreGrupo;
}
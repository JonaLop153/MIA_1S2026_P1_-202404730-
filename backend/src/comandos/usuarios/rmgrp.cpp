#include "rmgrp.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

map<string, string> RmGrp::parsearParametros(const string& comando) {
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

bool RmGrp::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("name") == params.end()) { error = "Error: -name obligatorio"; return false; }
    return true;
}

string RmGrp::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa";
    if (getUsuarioActual() != "root") return "Error: Solo root puede eliminar grupos";
    
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
    
    ifstream inodeFile(diskPath, ios::binary | ios::in);
    inodeFile.seekg(sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
    Inodo inodoUsers; inodeFile.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo)); inodeFile.close();
    
    int bloqueUsers = inodoUsers.i_block[0];
    ifstream blockFile(diskPath, ios::binary | ios::in);
    blockFile.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    BloqueArchivo bloqueContent; blockFile.read(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo)); blockFile.close();
    
    string contenido(bloqueContent.b_content);
    
    // ✅ Buscar grupo y marcar como eliminado (GID = 0)
    bool grupoEncontrado = false;
    istringstream iss(contenido);
    string linea;
    string nuevoContenido = "";
    
    while (getline(iss, linea)) {
        // Limpiar línea completamente
        linea.erase(0, linea.find_first_not_of(" \t\r\n"));
        linea.erase(linea.find_last_not_of(" \t\r\n") + 1);
        if (linea.empty()) continue;
        
        istringstream lineStream(linea);
        string gidStr, type, grupo;
        getline(lineStream, gidStr, ',');
        getline(lineStream, type, ',');
        
        if (type.find('G') != string::npos) {
            getline(lineStream, grupo, ',');
            // Limpiar espacios del nombre del grupo
            grupo.erase(0, grupo.find_first_not_of(" \t"));
            grupo.erase(grupo.find_last_not_of(" \t\r\n") + 1);
            
            if (grupo == nombreGrupo) {
                grupoEncontrado = true;
                // ✅ Marcar como eliminado: GID = 0
                nuevoContenido += "0,G," + grupo + "\n";
            } else {
                nuevoContenido += linea + "\n";
            }
        } else {
            // Líneas de usuario, mantener intactas
            nuevoContenido += linea + "\n";
        }
    }
    
    if (!grupoEncontrado) return "Error: Grupo no encontrado: " + nombreGrupo;
    
    // Escribir de vuelta
    memset(bloqueContent.b_content, 0, 64);
    strncpy(bloqueContent.b_content, nuevoContenido.c_str(), 63);
    
    fstream writeBlock(diskPath, ios::binary | ios::in | ios::out);
    writeBlock.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    writeBlock.write(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
    writeBlock.close();
    
    return "Grupo eliminado exitosamente: " + nombreGrupo;
}
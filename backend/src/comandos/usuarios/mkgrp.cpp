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
    if (!haySesionActiva()) return "Error: No hay sesión activa";
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
    
    // Leer inodo 3
    ifstream inodeFile(diskPath, ios::binary | ios::in);
    inodeFile.seekg(partStart + sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
    Inodo inodoUsers; inodeFile.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo)); inodeFile.close();
    
    int bloqueUsers = inodoUsers.i_block[0];
    ifstream blockFile(diskPath, ios::binary | ios::in);
    blockFile.seekg(partStart + sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    BloqueArchivo bloqueContent; blockFile.read(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo)); blockFile.close();
    
    // ✅ Leer contenido existente
    string contenido(bloqueContent.b_content);
    
    // ✅ Validar que el grupo no exista
    istringstream iss(contenido);
    string linea;
    while (getline(iss, linea)) {
        // Limpiar línea
        while (!linea.empty() && (linea.back() == '\n' || linea.back() == '\r' || linea.back() == ' ')) {
            linea.pop_back();
        }
        if (linea.empty()) continue;
        
        istringstream lineStream(linea);
        string gidStr, type, grupo;
        getline(lineStream, gidStr, ',');
        getline(lineStream, type, ',');
        
        if (type.find('G') != string::npos) {
            getline(lineStream, grupo, ',');
            while (!grupo.empty() && (grupo.front() == ' ' || grupo.front() == '\t')) grupo.erase(0, 1);
            while (!grupo.empty() && (grupo.back() == ' ' || grupo.back() == '\t')) grupo.pop_back();
            
            if (grupo == nombreGrupo) {
                return "Error: El grupo ya existe: " + nombreGrupo;
            }
        }
    }
    
    // ✅ Calcular nuevo GID
    int nuevoGID = 2;
    istringstream maxIss(contenido);
    string maxLinea;
    while (getline(maxIss, maxLinea)) {
        while (!maxLinea.empty() && (maxLinea.back() == '\n' || maxLinea.back() == '\r')) maxLinea.pop_back();
        if (maxLinea.empty()) continue;
        
        istringstream maxStream(maxLinea);
        string gidStr, gtype;
        getline(maxStream, gidStr, ',');
        getline(maxStream, gtype, ',');
        
        if (gtype.find('G') != string::npos) {
            try {
                int gid = stoi(gidStr);
                if (gid >= nuevoGID) nuevoGID = gid + 1;
            } catch (...) {}
        }
    }
    
    // ✅ Agregar nueva línea (formato compacto)
    string nuevaLinea = to_string(nuevoGID) + ",G," + nombreGrupo + "\n";
    
    // ✅ Verificar espacio (63 bytes máximo + null)
    if (contenido.length() + nuevaLinea.length() > 63) {
        return "Error: No hay espacio en users.txt para más grupos";
    }
    
    // ✅ Asegurar que contenido termine con \n
    if (!contenido.empty() && contenido.back() != '\n') {
        contenido += "\n";
    }
    contenido += nuevaLinea;
    
    // ✅ Escribir de vuelta
    memset(bloqueContent.b_content, 0, 64);
    strncpy(bloqueContent.b_content, contenido.c_str(), 63);
    
    fstream writeBlock(diskPath, ios::binary | ios::in | ios::out);
    writeBlock.seekg(partStart + sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    writeBlock.write(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
    writeBlock.close();
    
    return "Grupo creado exitosamente: " + nombreGrupo;
}
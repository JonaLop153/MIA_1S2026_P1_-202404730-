#include "login.h"
#include "../../structs.h"
#include "../../session/session.h"
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
    if (params.find("user") == params.end()) { error = "Error: -user obligatorio"; return false; }
    if (params.find("pass") == params.end()) { error = "Error: -pass obligatorio"; return false; }
    if (params.find("id") == params.end()) { error = "Error: -id obligatorio"; return false; }
    return true;
}

bool Login::autenticarUsuario(const string& id, const string& user, const string& pass, string& usersTxtPath) {
    MountMap& montadas = getParticionesMontadas();
    if (montadas.find(id) == montadas.end()) return false;
    
    string diskPath = montadas.at(id).first;
    int indice = montadas.at(id).second;
    
    // Leer MBR y SuperBlock
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return false;
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    int partStart = mbr.mbr_partitions[indice].part_start;
    file.close();
    
    ifstream sbFile(diskPath, ios::binary | ios::in);
    sbFile.seekg(partStart, ios::beg);
    SuperBlock sb;
    sbFile.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    sbFile.close();
    
    // Leer inodo 3 (users.txt)
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
    
    string contenido(bloqueContent.b_content);
    istringstream iss(contenido);
    string linea;
    
    while (getline(iss, linea)) {
        // ✅ Limpiar línea completamente
        linea.erase(0, linea.find_first_not_of(" \t\r\n"));
        linea.erase(linea.find_last_not_of(" \t\r\n") + 1);
        if (linea.empty()) continue;
        
        istringstream lineStream(linea);
        string uidStr, type, grupo, username, password;
        getline(lineStream, uidStr, ',');
        getline(lineStream, type, ',');
        
        // ✅ Solo usuarios (no grupos), y que no estén eliminados (UID != 0)
        if (type.find('U') != string::npos && uidStr != "0") {
            getline(lineStream, grupo, ',');
            getline(lineStream, username, ',');
            getline(lineStream, password);
            
            // ✅ Limpiar espacios
            username.erase(0, username.find_first_not_of(" \t"));
            username.erase(username.find_last_not_of(" \t\r\n") + 1);
            password.erase(0, password.find_first_not_of(" \t"));
            password.erase(password.find_last_not_of(" \t\r\n") + 1);
            
            // ✅ Comparar (case-sensitive según enunciado)
            if (username == user && password == pass) {
                usersTxtPath = diskPath;
                return true;
            }
        }
    }
    return false;
}

string Login::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    if (haySesionActiva()) return "Error: Ya hay sesión activa. Haga logout primero";
    
    string user = params["user"];
    string pass = params["pass"];
    string id = params["id"];
    string usersTxtPath;
    
    if (!autenticarUsuario(id, user, pass, usersTxtPath)) {
        return "Error: Autenticación fallida. Usuario o contraseña incorrectos";
    }
    
    iniciarSesion(user, pass, id);
    
    ostringstream oss;
    oss << "Sesión iniciada exitosamente:\n";
    oss << "  Usuario: " << user << "\n";
    oss << "  ID Partición: " << id;
    return oss.str();
}
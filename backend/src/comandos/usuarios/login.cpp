#include "login.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

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

bool Login::autenticarUsuario(const string& id, const string& user, const string& pass, string& diskPath) {
    // Buscar la partición montada
    MountMap& montadas = getParticionesMontadas();
    
    if (montadas.find(id) == montadas.end()) {
        return false;
    }
    
    diskPath = montadas.at(id).first;
    int indice = montadas.at(id).second;
    
    // ✅ Abrir archivo del disco en modo binario
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) {
        std::cerr << "DEBUG: No se pudo abrir disco: " << diskPath << std::endl;
        return false;
    }
    
    // ✅ Paso 1: Leer MBR desde el inicio del archivo (byte 0)
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    
    // ✅ Paso 2: Obtener el inicio de la partición desde el MBR
    int partStart = mbr.mbr_partitions[indice].part_start;
    std::cerr << "DEBUG: partStart=" << partStart << std::endl;
    
    // ✅ Paso 3: Ir al SuperBlock (está al inicio de la partición)
    file.seekg(partStart, ios::beg);
    
    // ✅ Paso 4: Leer SuperBlock
    SuperBlock sb;
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    
    std::cerr << "DEBUG SB: inode_start=" << sb.s_inode_start 
              << " block_start=" << sb.s_block_start 
              << " block_s=" << sb.s_block_s << std::endl;
    
    // ✅ Paso 5: Calcular offset para inodo 3 (users.txt)
    // s_inode_start es RELATIVO al inicio de la partición
    int inodeOffset = partStart + sb.s_inode_start + (3 * sizeof(Inodo));
    std::cerr << "DEBUG: inodeOffset para inodo 3 = " << inodeOffset << std::endl;
    
    // ✅ Paso 6: Leer inodo 3
    file.seekg(inodeOffset, ios::beg);
    Inodo inodoUsers;
    file.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo));
    
    std::cerr << "DEBUG Inodo3: i_block[0]=" << inodoUsers.i_block[0] 
              << " i_type=" << (int)inodoUsers.i_type << std::endl;
    
    // ✅ Paso 7: Calcular offset para el bloque de users.txt
    // i_block[0] es el NÚMERO de bloque (índice), no offset en bytes
    int bloqueNum = inodoUsers.i_block[0];
    if (bloqueNum < 0) {
        std::cerr << "DEBUG: bloqueNum invalido: " << bloqueNum << std::endl;
        file.close();
        return false;
    }
    
    // s_block_start es RELATIVO al inicio de la partición
    int bloqueOffset = partStart + sb.s_block_start + (bloqueNum * sb.s_block_s);
    std::cerr << "DEBUG: bloqueOffset = " << bloqueOffset << std::endl;
    
    // ✅ Paso 8: Leer contenido del bloque
    file.seekg(bloqueOffset, ios::beg);
    
    char buffer[64];
    memset(buffer, 0, 64);
    file.read(buffer, 64);
    
    file.close();
    
    // ✅ DEBUG: Mostrar contenido leído
    std::cerr << "DEBUG RAW CONTENT: '";
    for (int i = 0; i < 64; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) std::cerr << buffer[i];
        else if (buffer[i] == '\n') std::cerr << "\\n";
        else if (buffer[i] == '\r') std::cerr << "\\r";
        else if (buffer[i] == '\0') std::cerr << "\\0";
        else std::cerr << ".";
    }
    std::cerr << "'" << std::endl;
    
    // ✅ Paso 9: Parsear contenido línea por línea
    string contenido(buffer);
    istringstream iss(contenido);
    string linea;
    
    while (getline(iss, linea)) {
        // Limpiar saltos de línea y espacios
        while (!linea.empty() && (linea.back() == '\n' || linea.back() == '\r' || linea.back() == ' ' || linea.back() == '\t')) {
            linea.pop_back();
        }
        
        if (linea.empty()) continue;
        
        // ✅ Parsear línea: UID,Tipo,Grupo,Usuario,Password
        istringstream lineStream(linea);
        string uidStr, type, grupo, username, password;
        
        getline(lineStream, uidStr, ',');
        getline(lineStream, type, ',');
        
        // ✅ Buscar solo usuarios (tipo U), no grupos (tipo G)
        if (type.find('U') != string::npos) {
            getline(lineStream, grupo, ',');
            getline(lineStream, username, ',');
            getline(lineStream, password);
            
            // ✅ Limpiar espacios en blanco
            while (!username.empty() && (username.front() == ' ' || username.front() == '\t')) {
                username.erase(0, 1);
            }
            while (!username.empty() && (username.back() == ' ' || username.back() == '\t' || username.back() == '\r' || username.back() == '\n')) {
                username.pop_back();
            }
            
            while (!password.empty() && (password.front() == ' ' || password.front() == '\t')) {
                password.erase(0, 1);
            }
            while (!password.empty() && (password.back() == ' ' || password.back() == '\t' || password.back() == '\r' || password.back() == '\n')) {
                password.pop_back();
            }
            
            std::cerr << "DEBUG: Comparando user='" << username << "' pass='" << password << "' uid='" << uidStr << "'" << std::endl;
            std::cerr << "DEBUG: Buscando user='" << user << "' pass='" << pass << "'" << std::endl;
            
            // ✅ Comparar usuario y contraseña (case-sensitive)
            // Verificar que no esté eliminado (uidStr != "0")
            if (username == user && password == pass && uidStr != "0") {
                std::cerr << "DEBUG: AUTENTICACIÓN EXITOSA" << std::endl;
                return true;
            }
        }
    }
    
    std::cerr << "DEBUG: AUTENTICACIÓN FALLIDA - usuario no encontrado" << std::endl;
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
    string diskPath;
    
    if (!autenticarUsuario(id, user, pass, diskPath)) {
        return "Error: Autenticación fallida. Usuario o contraseña incorrectos";
    }
    
    // Iniciar sesión
    iniciarSesion(user, pass, id, diskPath);
    
    ostringstream oss;
    oss << "Sesión iniciada exitosamente:\n";
    oss << "  Usuario: " << user << "\n";
    oss << "  ID Partición: " << id;
    
    return oss.str();
}
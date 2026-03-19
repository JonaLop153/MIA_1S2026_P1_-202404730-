#include "mkusr.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

map<string, string> MkUsr::parsearParametros(const string& comando) {
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

bool MkUsr::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("user") == params.end()) { error = "Error: -user obligatorio"; return false; }
    if (params.find("pass") == params.end()) { error = "Error: -pass obligatorio"; return false; }
    if (params.find("grp") == params.end()) { error = "Error: -grp obligatorio"; return false; }
    return true;
}

string MkUsr::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa";
    if (getUsuarioActual() != "root") return "Error: Solo root puede crear usuarios";
    
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    string user = params["user"], pass = params["pass"], grp = params["grp"];
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
    
    // ✅ Validar que el usuario NO exista (incluso si está eliminado)
    istringstream checkIss(contenido);
    string checkLinea;
    while (getline(checkIss, checkLinea)) {
        checkLinea.erase(0, checkLinea.find_first_not_of(" \t\r\n"));
        checkLinea.erase(checkLinea.find_last_not_of(" \t\r\n") + 1);
        if (checkLinea.empty()) continue;
        
        istringstream checkStream(checkLinea);
        string uidStr, type, grupo, username;
        getline(checkStream, uidStr, ',');
        getline(checkStream, type, ',');
        
        if (type.find('U') != string::npos) {
            getline(checkStream, grupo, ',');
            getline(checkStream, username, ',');
            username.erase(0, username.find_first_not_of(" \t"));
            username.erase(username.find_last_not_of(" \t\r\n") + 1);
            
            if (username == user) {
                return "Error: El usuario ya existe: " + user;
            }
        }
    }
    
    // ✅ Validar que el grupo exista
    bool grupoExiste = false;
    istringstream grpIss(contenido);
    string grpLinea;
    while (getline(grpIss, grpLinea)) {
        grpLinea.erase(0, grpLinea.find_first_not_of(" \t\r\n"));
        grpLinea.erase(grpLinea.find_last_not_of(" \t\r\n") + 1);
        if (grpLinea.empty()) continue;
        
        istringstream grpStream(grpLinea);
        string gidStr, gtype, gnombre;
        getline(grpStream, gidStr, ',');
        getline(grpStream, gtype, ',');
        
        if (gtype.find('G') != string::npos) {
            getline(grpStream, gnombre, ',');
            gnombre.erase(0, gnombre.find_first_not_of(" \t"));
            gnombre.erase(gnombre.find_last_not_of(" \t\r\n") + 1);
            
            if (gnombre == grp) {
                grupoExiste = true;
                break;
            }
        }
    }
    if (!grupoExiste) return "Error: El grupo no existe: " + grp;
    
    // Calcular nuevo UID
    int nuevoUID = 2;
    size_t pos = contenido.rfind('\n');
    if (pos != string::npos) {
        string ultimaLinea = contenido.substr(pos + 1);
        istringstream lastIss(ultimaLinea);
        string idStr; getline(lastIss, idStr, ',');
        try { nuevoUID = stoi(idStr) + 1; } catch (...) { nuevoUID = 2; }
    }
    
    // Agregar nueva línea de usuario
    string nuevaLinea = "\n" + to_string(nuevoUID) + ",U," + grp + "," + user + "," + pass;
    string nuevoContenido = contenido + nuevaLinea;
    
    memset(bloqueContent.b_content, 0, 64);
    strncpy(bloqueContent.b_content, nuevoContenido.c_str(), 63);
    bloqueContent.b_content[63] = '\0';
    
    fstream writeBlock(diskPath, ios::binary | ios::in | ios::out);
    writeBlock.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
    writeBlock.write(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
    writeBlock.close();
    
    return "Usuario creado exitosamente: " + user;
}
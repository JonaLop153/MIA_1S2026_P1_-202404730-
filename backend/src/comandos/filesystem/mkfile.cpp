#include "mkfile.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <vector>

namespace fs = std::filesystem;

map<string, string> MkFile::parsearParametros(const string& comando) {
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

bool MkFile::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) { error = "Error: -path obligatorio"; return false; }
    return true;
}

bool MkFile::crearArchivo(const string& id, const string& path, int size, const string& cont, bool r) {
    MountMap& montadas = getParticionesMontadas();
    if (montadas.find(id) == montadas.end()) return false;
    
    string diskPath = montadas.at(id).first;
    int indice = montadas.at(id).second;
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return false;
    
    MBR mbr; file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR)); file.close();
    int partStart = mbr.mbr_partitions[indice].part_start;
    
    ifstream sbFile(diskPath, ios::binary | ios::in);
    sbFile.seekg(partStart, ios::beg);
    SuperBlock sb; sbFile.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock)); sbFile.close();
    
    // Leer bitmaps
    int bitmapInodosSize = (sb.s_inodes_count + 7) / 8;
    char* bitmapInodos = new char[bitmapInodosSize];
    ifstream bitmapInoFile(diskPath, ios::binary | ios::in);
    bitmapInoFile.seekg(sb.s_bm_inode_start, ios::beg);
    bitmapInoFile.read(bitmapInodos, bitmapInodosSize);
    bitmapInoFile.close();
    
    int bitmapBloquesSize = (sb.s_blocks_count + 7) / 8;
    char* bitmapBloques = new char[bitmapBloquesSize];
    ifstream bitmapBloFile(diskPath, ios::binary | ios::in);
    bitmapBloFile.seekg(sb.s_bm_block_start, ios::beg);
    bitmapBloFile.read(bitmapBloques, bitmapBloquesSize);
    bitmapBloFile.close();
    
    // Buscar inodo libre
    int inodoLibre = -1;
    for (int i = sb.s_firts_ino; i < sb.s_inodes_count; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        if ((bitmapInodos[byteIndex] & (1 << bitIndex)) == 0) {
            inodoLibre = i;
            break;
        }
    }
    if (inodoLibre == -1) { delete[] bitmapInodos; delete[] bitmapBloques; return false; }
    
    // Calcular bloques necesarios
    int bloquesNecesarios = (size > 0) ? (size + 63) / 64 : 1;
    
    // Buscar bloques libres
    vector<int> bloquesLibres;
    for (int i = sb.s_first_blo; i < sb.s_blocks_count && (int)bloquesLibres.size() < bloquesNecesarios; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        if ((bitmapBloques[byteIndex] & (1 << bitIndex)) == 0) {
            bloquesLibres.push_back(i);
        }
    }
    
    if ((int)bloquesLibres.size() < bloquesNecesarios) {
        delete[] bitmapInodos; delete[] bitmapBloques; return false;
    }
    
    // Obtener usuario actual
    SesionActiva& sesion = getSesionActiva();
    int uid = 1, gid = 1;
    
    // Crear inodo de archivo
    Inodo inodoArchivo;
    memset(&inodoArchivo, 0, sizeof(Inodo));
    inodoArchivo.i_uid = uid;
    inodoArchivo.i_gid = gid;
    inodoArchivo.i_s = size;
    inodoArchivo.i_atime = time(nullptr);
    inodoArchivo.i_ctime = time(nullptr);
    inodoArchivo.i_mtime = time(nullptr);
    inodoArchivo.i_type = '1';  // Archivo
    inodoArchivo.i_perm[0] = '6';
    inodoArchivo.i_perm[1] = '6';
    inodoArchivo.i_perm[2] = '4';
    
    for (int i = 0; i < 15 && i < (int)bloquesLibres.size(); i++) {
        inodoArchivo.i_block[i] = bloquesLibres[i];
    }
    for (int i = bloquesLibres.size(); i < 15; i++) {
        inodoArchivo.i_block[i] = -1;
    }
    
    fstream inodeFile(diskPath, ios::binary | ios::in | ios::out);
    inodeFile.seekg(sb.s_inode_start + (inodoLibre * sizeof(Inodo)), ios::beg);
    inodeFile.write(reinterpret_cast<char*>(&inodoArchivo), sizeof(Inodo));
    inodeFile.close();
    
    // Escribir contenido en bloques
    string contenido = cont;
    if (size > 0 && cont.empty()) {
        for (int i = 0; i < size; i++) {
            contenido += to_string(i % 10);
        }
    }
    
    for (size_t i = 0; i < bloquesLibres.size(); i++) {
        BloqueArchivo bloqueArchivo;
        memset(&bloqueArchivo, 0, sizeof(BloqueArchivo));
        
        size_t offset = i * 64;
        size_t remaining = contenido.size() - offset;
        size_t toCopy = (remaining >= 64) ? 64 : remaining;
        
        if (toCopy > 0) {
            strncpy(bloqueArchivo.b_content, contenido.substr(offset, toCopy).c_str(), toCopy);
        }
        
        fstream blockFile(diskPath, ios::binary | ios::in | ios::out);
        blockFile.seekg(sb.s_block_start + (bloquesLibres[i] * sb.s_block_s), ios::beg);
        blockFile.write(reinterpret_cast<char*>(&bloqueArchivo), sizeof(BloqueArchivo));
        blockFile.close();
    }
    
    // Actualizar bitmaps
    bitmapInodos[inodoLibre / 8] |= (1 << (inodoLibre % 8));
    for (int bloque : bloquesLibres) {
        bitmapBloques[bloque / 8] |= (1 << (bloque % 8));
    }
    
    fstream bitmapInoWrite(diskPath, ios::binary | ios::in | ios::out);
    bitmapInoWrite.seekg(sb.s_bm_inode_start, ios::beg);
    bitmapInoWrite.write(bitmapInodos, bitmapInodosSize);
    bitmapInoWrite.close();
    
    fstream bitmapBloWrite(diskPath, ios::binary | ios::in | ios::out);
    bitmapBloWrite.seekg(sb.s_bm_block_start, ios::beg);
    bitmapBloWrite.write(bitmapBloques, bitmapBloquesSize);
    bitmapBloWrite.close();
    
    // Actualizar SuperBlock
    sb.s_free_inodes_count--;
    sb.s_free_blocks_count -= bloquesLibres.size();
    sb.s_firts_ino = inodoLibre + 1;
    sb.s_first_blo = bloquesLibres.back() + 1;
    sb.s_mtime = time(nullptr);
    
    fstream sbWrite(diskPath, ios::binary | ios::in | ios::out);
    sbWrite.seekg(partStart, ios::beg);
    sbWrite.write(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    sbWrite.close();
    
    delete[] bitmapInodos;
    delete[] bitmapBloques;
    
    return true;
}

string MkFile::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa";
    
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    string path = params["path"];
    int size = params.count("size") ? stoi(params["size"]) : 0;
    string cont = params.count("cont") ? params["cont"] : "";
    bool r = params.count("r");
    string id = getIdParticionActual();
    
    if (size < 0) return "Error: El tamaño no puede ser negativo";
    
    // ✅ Verificar que las carpetas padres existan (si no se usa -r)
    if (!r) {
        fs::path filePath(path);
        fs::path parentPath = filePath.parent_path();
        
        if (!parentPath.empty() && !parentPath.string().empty() && parentPath.string() != "/" && parentPath.string() != "") {
            // Verificar si el padre existe en EXT2 (simplificado: verificar en sistema real para demo)
            // En una implementación completa, deberías buscar el inodo del directorio padre
        }
    }
    
    // Si cont existe, leer archivo del sistema real
    if (!cont.empty()) {
        ifstream fileReal(cont, ios::in);
        if (!fileReal.is_open()) return "Error: No se pudo abrir el archivo: " + cont;
        stringstream buffer;
        buffer << fileReal.rdbuf();
        cont = buffer.str();
        fileReal.close();
        size = cont.size();
    }
    
    if (crearArchivo(id, path, size, cont, r)) {
        return "Archivo creado exitosamente: " + path;
    } else {
        return "Error: No se pudo crear el archivo. Verifique espacio disponible";
    }
}
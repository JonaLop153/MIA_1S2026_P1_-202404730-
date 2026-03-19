#include "mkfs.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <cmath>

namespace fs = std::filesystem;

map<string, string> MKFS::parsearParametros(const string& comando) {
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

bool MKFS::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("id") == params.end()) {
        error = "Error: El parámetro -id es obligatorio";
        return false;
    }
    
    if (params.count("type")) {
        string type = params.at("type");
        for (char& c : type) c = tolower(c);
        if (type != "full" && type != "fast") {
            error = "Error: El parámetro -type solo acepta full o fast";
            return false;
        }
    }
    
    return true;
}

bool MKFS::buscarParticionMontada(const string& id, string& path, int& indice) {
    MountMap& montadas = getParticionesMontadas();
    
    for (auto const& [mountId, data] : montadas) {
        if (mountId == id) {
            path = data.first;
            indice = data.second;
            return true;
        }
    }
    return false;
}

bool MKFS::crearUsersTXT(const string& path, int partStart, int inodeTableOffset, int blockTableOffset, int blockSize, int nextFreeBlock) {
    fstream file(path, ios::binary | ios::in | ios::out);
    if (!file.is_open()) return false;
    
    // ✅ Contenido inicial con root
    string contenido = "1,G,root\n1,U,root,root,123\n";
    
    // ✅ Inodo 3 para users.txt
    Inodo inodoUsers;
    memset(&inodoUsers, 0, sizeof(Inodo));
    inodoUsers.i_uid = 1;
    inodoUsers.i_gid = 1;
    inodoUsers.i_s = contenido.size();
    inodoUsers.i_atime = time(nullptr);
    inodoUsers.i_ctime = time(nullptr);
    inodoUsers.i_mtime = time(nullptr);
    inodoUsers.i_type = '1';  // 1 = Archivo
    inodoUsers.i_perm[0] = '6';
    inodoUsers.i_perm[1] = '6';
    inodoUsers.i_perm[2] = '4';
    
    // ✅ Asignar primer bloque para users.txt
    inodoUsers.i_block[0] = nextFreeBlock;
    for (int i = 1; i < 15; i++) {
        inodoUsers.i_block[i] = -1;
    }
    
    // Escribir inodo 3
    file.seekp(partStart + inodeTableOffset + (3 * sizeof(Inodo)), ios::beg);
    file.write(reinterpret_cast<const char*>(&inodoUsers), sizeof(Inodo));
    
    // ✅ Escribir contenido en el bloque
    BloqueArchivo bloqueUsers;
    memset(&bloqueUsers, 0, sizeof(BloqueArchivo));
    strncpy(bloqueUsers.b_content, contenido.c_str(), 63);
    bloqueUsers.b_content[63] = '\0';
    
    file.seekp(partStart + blockTableOffset + (nextFreeBlock * blockSize), ios::beg);
    file.write(reinterpret_cast<const char*>(&bloqueUsers), sizeof(BloqueArchivo));
    
    file.close();
    return true;
}

string MKFS::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string id = params["id"];
    string type = params.count("type") ? params["type"] : "full";
    for (char& c : type) c = tolower(c);
    
    string path;
    int indice = -1;
    if (!buscarParticionMontada(id, path, indice)) {
        return "Error: No hay una partición montada con ID: " + id;
    }
    
    if (!fs::exists(path)) {
        return "Error: El disco no existe en: " + path;
    }
    
    // ✅ Leer MBR para obtener información de la partición
    ifstream file(path, ios::binary | ios::in);
    if (!file.is_open()) {
        return "Error: No se pudo abrir el disco";
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    if (indice < 0 || indice >= 4) {
        return "Error: Índice de partición inválido";
    }
    
    int partStart = mbr.mbr_partitions[indice].part_start;
    int partSize = mbr.mbr_partitions[indice].part_size;
    
    if (partStart <= 0 || partSize <= 0) {
        return "Error: Partición inválida";
    }
    
    // =====================
    // CÁLCULO DE ESTRUCTURAS EXT2
    // =====================
    
    int blockSize = 64;  // Según enunciado
    int sbSize = sizeof(SuperBlock);
    int inodeSize = sizeof(Inodo);
    
    // Calcular número de estructuras
    int espacioDisponible = partSize - sbSize;
    int estructurasBase = floor(espacioDisponible / (1 + 3 + inodeSize + 3 * blockSize));
    
    int totalInodos = max(10, min(estructurasBase, 100));
    int totalBloques = totalInodos * 3;
    
    // Calcular offsets RELATIVOS al inicio de la partición
    int superBlockOffset = 0;
    int bitmapInodosOffset = sbSize;
    int bitmapBloquesOffset = bitmapInodosOffset + ((totalInodos + 7) / 8);
    int inodeTableOffset = bitmapBloquesOffset + ((totalBloques + 7) / 8);
    int blockTableOffset = inodeTableOffset + (totalInodos * inodeSize);
    
    // =====================
    // CREAR SUPERBLOCK
    // =====================
    
    SuperBlock sb;
    memset(&sb, 0, sizeof(SuperBlock));
    
    sb.s_filesystem_type = 2;  // EXT2
    sb.s_inodes_count = totalInodos;
    sb.s_blocks_count = totalBloques;
    sb.s_free_inodes_count = totalInodos - 4;  // 0, 1, 2 (raíz), 3 (users.txt)
    sb.s_free_blocks_count = totalBloques - 4;  // 0, 1, 2, 3
    sb.s_mtime = time(nullptr);
    sb.s_umtime = 0;
    sb.s_mnt_count = 0;
    sb.s_magic = 0xEF53;
    sb.s_inode_s = inodeSize;
    sb.s_block_s = blockSize;
    sb.s_firts_ino = 4;
    sb.s_first_blo = 4;
    sb.s_bm_inode_start = bitmapInodosOffset;
    sb.s_bm_block_start = bitmapBloquesOffset;
    sb.s_inode_start = inodeTableOffset;
    sb.s_block_start = blockTableOffset;
    
    // Escribir SuperBlock
    fstream sbFile(path, ios::binary | ios::in | ios::out);
    sbFile.seekp(partStart + superBlockOffset, ios::beg);
    sbFile.write(reinterpret_cast<const char*>(&sb), sizeof(SuperBlock));
    sbFile.close();
    
    // =====================
    // ESCRIBIR BITMAPS
    // =====================
    
    fstream bitmapFile(path, ios::binary | ios::in | ios::out);
    
    // Bitmap de inodos: marcar 0, 1, 2, 3 como ocupados
    int bitmapInodosSize = (totalInodos + 7) / 8;
    char* bitmapInodos = new char[bitmapInodosSize]();
    bitmapInodos[0] = 0b00001111;  // Bits 0, 1, 2, 3 = 1
    
    bitmapFile.seekp(partStart + bitmapInodosOffset, ios::beg);
    bitmapFile.write(bitmapInodos, bitmapInodosSize);
    delete[] bitmapInodos;
    
    // Bitmap de bloques: marcar 0, 1, 2, 3 como ocupados
    int bitmapBloquesSize = (totalBloques + 7) / 8;
    char* bitmapBloques = new char[bitmapBloquesSize]();
    bitmapBloques[0] = 0b00001111;  // Bits 0, 1, 2, 3 = 1
    
    bitmapFile.seekp(partStart + bitmapBloquesOffset, ios::beg);
    bitmapFile.write(bitmapBloques, bitmapBloquesSize);
    delete[] bitmapBloques;
    
    bitmapFile.close();
    
    // =====================
    // ESCRIBIR TABLA DE INODOS
    // =====================
    
    fstream inodeFile(path, ios::binary | ios::in | ios::out);
    
    // Inodo 2: raíz (/)
    Inodo inodoRaiz;
    memset(&inodoRaiz, 0, sizeof(Inodo));
    inodoRaiz.i_uid = 1;
    inodoRaiz.i_gid = 1;
    inodoRaiz.i_s = blockSize;
    inodoRaiz.i_atime = time(nullptr);
    inodoRaiz.i_ctime = time(nullptr);
    inodoRaiz.i_mtime = time(nullptr);
    inodoRaiz.i_type = '0';  // 0 = Carpeta
    inodoRaiz.i_perm[0] = '7';
    inodoRaiz.i_perm[1] = '7';
    inodoRaiz.i_perm[2] = '7';
    inodoRaiz.i_block[0] = 0;  // Apunta al bloque 0
    for (int i = 1; i < 15; i++) {
        inodoRaiz.i_block[i] = -1;
    }
    
    inodeFile.seekp(partStart + inodeTableOffset + (2 * inodeSize), ios::beg);
    inodeFile.write(reinterpret_cast<const char*>(&inodoRaiz), sizeof(Inodo));
    
    inodeFile.close();
    
    // =====================
    // ESCRIBIR BLOQUES
    // =====================
    
    fstream blockFile(path, ios::binary | ios::in | ios::out);
    
    // Bloque 0: Carpeta raíz (/) con "." y ".."
    BloqueCarpeta bloqueRaiz;
    memset(&bloqueRaiz, 0, sizeof(BloqueCarpeta));
    
    strncpy(bloqueRaiz.b_content[0].b_name, ".", 12);
    bloqueRaiz.b_content[0].b_inodo = 2;
    
    strncpy(bloqueRaiz.b_content[1].b_name, "..", 12);
    bloqueRaiz.b_content[1].b_inodo = 2;
    
    blockFile.seekp(partStart + blockTableOffset + (0 * blockSize), ios::beg);
    blockFile.write(reinterpret_cast<const char*>(&bloqueRaiz), sizeof(BloqueCarpeta));
    
    blockFile.close();
    
    // =====================
    // CREAR users.txt (BLOQUE 3)
    // =====================
    
    if (!crearUsersTXT(path, partStart, inodeTableOffset, blockTableOffset, blockSize, 3)) {
        return "Error: No se pudo crear users.txt";
    }
    
    // =====================
    // RESPUESTA
    // =====================
    
    ostringstream oss;
    oss << "Partición formateada con EXT2 exitosamente:\n";
    oss << "  ID: " << id << "\n";
    oss << "  Type: " << type << "\n";
    oss << "  Inodos totales: " << totalInodos << "\n";
    oss << "  Bloques totales: " << totalBloques << "\n";
    oss << "  Tamaño de bloque: " << blockSize << " bytes\n";
    oss << "  SuperBlock: byte " << partStart + superBlockOffset << "\n";
    oss << "  Bitmap Inodos: byte " << partStart + bitmapInodosOffset << "\n";
    oss << "  Bitmap Bloques: byte " << partStart + bitmapBloquesOffset << "\n";
    oss << "  Tabla Inodos: byte " << partStart + inodeTableOffset << "\n";
    oss << "  Tabla Bloques: byte " << partStart + blockTableOffset << "\n";
    oss << "  users.txt: creado en raíz (inodo 3, bloque 3)";
    
    return oss.str();
}
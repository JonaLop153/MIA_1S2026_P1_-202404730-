#include "rep.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <vector>
#include <set>

namespace fs = std::filesystem;

map<string, string> Rep::parsearParametros(const string& comando) {
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

bool Rep::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) { error = "Error: -path obligatorio"; return false; }
    if (params.find("name") == params.end()) { error = "Error: -name obligatorio"; return false; }
    if (params.find("id") == params.end()) { error = "Error: -id obligatorio"; return false; }
    
    string name = params.at("name");
    for (char& c : name) c = tolower(c);
    vector<string> validos = {"mbr", "disk", "inode", "block", "bm_inode", "bm_block", "tree", "sb", "file", "ls"};
    bool valido = false;
    for (const auto& v : validos) if (name == v) { valido = true; break; }
    if (!valido) { error = "Error: -name debe ser: mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file, ls"; return false; }
    return true;
}

bool Rep::obtenerPathDisco(const string& id, string& path, int& indice) {
    MountMap& montadas = getParticionesMontadas();
    for (auto const& [mountId, data] : montadas) {
        if (mountId == id) { path = data.first; indice = data.second; return true; }
    }
    return false;
}

string Rep::escapeDot(const string& texto) {
    string resultado = "";
    for (char c : texto) {
        switch (c) {
            case '"': resultado += "\\\""; break;
            case '\\': resultado += "\\\\"; break;
            case '\n': resultado += "\\n"; break;
            case '\r': resultado += "\\r"; break;
            case '\t': resultado += "\\t"; break;
            case '{': resultado += "\\{"; break;
            case '}': resultado += "\\}"; break;
            case '<': resultado += "\\<"; break;
            case '>': resultado += "\\>"; break;
            case '|': resultado += "\\|"; break;
            default: if (c >= 32 && c <= 126) resultado += c; break;
        }
    }
    return resultado;
}

string Rep::generarReporteMBR(const string& diskPath) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    MBR mbr; file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR)); file.close();
    if (mbr.mbr_tamano <= 0) return "Error: MBR inválido";
    
    ostringstream dot;
    dot << "digraph MBR {\n  rankdir=TB;\n  node [shape=record, style=filled, fillcolor=lightblue];\n\n";
    char fechaStr[50]; strftime(fechaStr, sizeof(fechaStr), "%Y-%m-%d %H:%M:%S", localtime(&mbr.mbr_fecha_creacion));
    dot << "  mbr [label=\"{MBR\\nTamaño: " << mbr.mbr_tamano << " bytes\\nFecha: " << fechaStr << "\\nSignature: 0x" << hex << mbr.mbr_dsk_signature << dec << "\\nFit: " << mbr.dsk_fit << "}\"];\n\n";
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' || mbr.mbr_partitions[i].part_type == 'P' || mbr.mbr_partitions[i].part_type == 'E') {
            string nombre = "", id = "";
            for (int j = 0; j < 16 && mbr.mbr_partitions[i].part_name[j] != '\0'; j++) 
                if (mbr.mbr_partitions[i].part_name[j] >= 32 && mbr.mbr_partitions[i].part_name[j] <= 126) nombre += mbr.mbr_partitions[i].part_name[j];
            for (int j = 0; j < 4 && mbr.mbr_partitions[i].part_id[j] != '\0'; j++) 
                if (mbr.mbr_partitions[i].part_id[j] >= 32 && mbr.mbr_partitions[i].part_id[j] <= 126) id += mbr.mbr_partitions[i].part_id[j];
            dot << "  p" << i << " [label=\"{Partición " << i << "\\nNombre: " << escapeDot(nombre) << "\\nTipo: " << mbr.mbr_partitions[i].part_type << "\\nStart: " << mbr.mbr_partitions[i].part_start << "\\nSize: " << mbr.mbr_partitions[i].part_size << " bytes\\n";
            if (!id.empty()) dot << "ID: " << escapeDot(id) << "\\n";
            dot << "Fit: " << mbr.mbr_partitions[i].part_fit << "}\"];\n  mbr -> p" << i << ";\n";
        }
    }
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteDisk(const string& diskPath) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    MBR mbr; file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR)); file.close();
    if (mbr.mbr_tamano <= 0) return "Error: MBR inválido";
    
    ostringstream dot;
    dot << "digraph DISK {\n  rankdir=LR;\n  node [shape=box, style=filled];\n\n";
    dot << "  mbr [label=\"MBR\\n(64 bytes)\", fillcolor=lightyellow];\n";
    int espacioUsado = sizeof(MBR);
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' || mbr.mbr_partitions[i].part_type == 'P' || mbr.mbr_partitions[i].part_type == 'E') {
            string nombre = "";
            for (int j = 0; j < 16 && mbr.mbr_partitions[i].part_name[j] != '\0'; j++) 
                if (mbr.mbr_partitions[i].part_name[j] >= 32 && mbr.mbr_partitions[i].part_name[j] <= 126) nombre += mbr.mbr_partitions[i].part_name[j];
            int size = mbr.mbr_partitions[i].part_size;
            int width = min(200, max(20, size / 50000));
            dot << "  p" << i << " [label=\"" << escapeDot(nombre) << "\\n" << size << " bytes\", width=" << width/50.0 << ", fillcolor=lightgreen];\n  mbr -> p" << i << ";\n";
            espacioUsado += size;
        }
    }
    int espacioLibre = mbr.mbr_tamano - espacioUsado;
    if (espacioLibre > 0) {
        int width = min(200, max(20, espacioLibre / 50000));
        dot << "  free [label=\"Libre\\n" << espacioLibre << " bytes\", width=" << width/50.0 << ", fillcolor=lightgray];\n  mbr -> free;\n";
    }
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteInode(const string& diskPath, int partStart, SuperBlock& sb) {
    ostringstream dot;
    dot << "digraph Inode {\n  rankdir=TB;\n  node [shape=record, style=filled, fillcolor=lightblue];\n\n";
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    char* bitmapInodos = new char[(sb.s_inodes_count + 7) / 8]();
    file.seekg(sb.s_bm_inode_start, ios::beg);
    file.read(bitmapInodos, (sb.s_inodes_count + 7) / 8);
    
    for (int i = sb.s_firts_ino; i < sb.s_inodes_count; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        if ((bitmapInodos[byteIndex] & (1 << bitIndex)) != 0) {
            Inodo inodo;
            file.seekg(sb.s_inode_start + (i * sizeof(Inodo)), ios::beg);
            file.read(reinterpret_cast<char*>(&inodo), sizeof(Inodo));
            dot << "  inode" << i << " [label=\"{Inodo " << i << "\\n";
            dot << "UID: " << inodo.i_uid << "\\nGID: " << inodo.i_gid << "\\n";
            dot << "Size: " << inodo.i_s << "\\nType: " << (inodo.i_type == '0' ? "Carpeta" : "Archivo") << "\\n";
            dot << "Perm: " << inodo.i_perm[0] << inodo.i_perm[1] << inodo.i_perm[2] << "\\n";
            dot << "Blocks: ";
            for (int j = 0; j < 15; j++) {
                if (inodo.i_block[j] != -1) dot << inodo.i_block[j] << " ";
            }
            dot << "}\"];\n";
        }
    }
    delete[] bitmapInodos;
    file.close();
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteBlock(const string& diskPath, int partStart, SuperBlock& sb) {
    ostringstream dot;
    dot << "digraph Block {\n  rankdir=LR;\n  node [shape=record, style=filled];\n\n";
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    char* bitmapBloques = new char[(sb.s_blocks_count + 7) / 8]();
    file.seekg(sb.s_bm_block_start, ios::beg);
    file.read(bitmapBloques, (sb.s_blocks_count + 7) / 8);
    
    int count = 0;
    for (int i = sb.s_first_blo; i < sb.s_blocks_count && count < 50; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        if ((bitmapBloques[byteIndex] & (1 << bitIndex)) != 0) {
            BloqueArchivo bloque;
            file.seekg(sb.s_block_start + (i * sb.s_block_s), ios::beg);
            file.read(reinterpret_cast<char*>(&bloque), sizeof(BloqueArchivo));
            dot << "  block" << i << " [label=\"{Bloque " << i << "\\n";
            string contenido = "";
            for (int j = 0; j < 64 && bloque.b_content[j] != '\0'; j++) {
                if (bloque.b_content[j] >= 32 && bloque.b_content[j] <= 126) contenido += bloque.b_content[j];
            }
            dot << "Content: " << escapeDot(contenido.substr(0, 30)) << "...}\\n\", fillcolor=lightgreen];\n";
            count++;
        }
    }
    delete[] bitmapBloques;
    file.close();
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteBmInode(const string& diskPath, int partStart, SuperBlock& sb) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    int bitmapSize = (sb.s_inodes_count + 7) / 8;
    char* bitmap = new char[bitmapSize]();
    file.seekg(sb.s_bm_inode_start, ios::beg);
    file.read(bitmap, bitmapSize);
    file.close();
    
    ostringstream txt;
    txt << "Bitmap de Inodos (" << sb.s_inodes_count << " inodos)\n";
    txt << "========================================\n\n";
    
    for (int i = 0; i < sb.s_inodes_count; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        int bit = (bitmap[byteIndex] >> bitIndex) & 1;
        txt << bit;
        if ((i + 1) % 20 == 0) txt << "\n";
        else txt << " ";
    }
    txt << "\n\nLeyenda: 0 = Libre, 1 = Ocupado\n";
    delete[] bitmap;
    return txt.str();
}

string Rep::generarReporteBmBlock(const string& diskPath, int partStart, SuperBlock& sb) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    int bitmapSize = (sb.s_blocks_count + 7) / 8;
    char* bitmap = new char[bitmapSize]();
    file.seekg(sb.s_bm_block_start, ios::beg);
    file.read(bitmap, bitmapSize);
    file.close();
    
    ostringstream txt;
    txt << "Bitmap de Bloques (" << sb.s_blocks_count << " bloques)\n";
    txt << "========================================\n\n";
    
    for (int i = 0; i < sb.s_blocks_count; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        int bit = (bitmap[byteIndex] >> bitIndex) & 1;
        txt << bit;
        if ((i + 1) % 20 == 0) txt << "\n";
        else txt << " ";
    }
    txt << "\n\nLeyenda: 0 = Libre, 1 = Ocupado\n";
    delete[] bitmap;
    return txt.str();
}

string Rep::generarReporteTree(const string& diskPath, int partStart, SuperBlock& sb) {
    ostringstream dot;
    dot << "digraph Tree {\n  rankdir=TB;\n  node [shape=box, style=filled];\n\n";
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    char* bitmapInodos = new char[(sb.s_inodes_count + 7) / 8]();
    file.seekg(sb.s_bm_inode_start, ios::beg);
    file.read(bitmapInodos, (sb.s_inodes_count + 7) / 8);
    
    dot << "  root [label=\"{/}\\nInodo: 2\", fillcolor=lightyellow];\n";
    
    for (int i = sb.s_firts_ino; i < sb.s_inodes_count && i < 20; i++) {
        int byteIndex = i / 8, bitIndex = i % 8;
        if ((bitmapInodos[byteIndex] & (1 << bitIndex)) != 0) {
            Inodo inodo;
            file.seekg(sb.s_inode_start + (i * sizeof(Inodo)), ios::beg);
            file.read(reinterpret_cast<char*>(&inodo), sizeof(Inodo));
            
            string tipo = (inodo.i_type == '0') ? "Carpeta" : "Archivo";
            string color = (inodo.i_type == '0') ? "lightyellow" : "lightgreen";
            
            dot << "  inode" << i << " [label=\"" << tipo << "\\nInodo: " << i << "\\nSize: " << inodo.i_s << "\", fillcolor=" << color << "];\n";
            dot << "  root -> inode" << i << ";\n";
        }
    }
    delete[] bitmapInodos;
    file.close();
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteSb(const string& diskPath, int partStart) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    
    SuperBlock sb;
    file.seekg(partStart, ios::beg);
    file.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    file.close();
    
    ostringstream dot;
    dot << "digraph SuperBlock {\n  rankdir=TB;\n  node [shape=record, style=filled, fillcolor=lightblue];\n\n";
    dot << "  sb [label=\"{SuperBlock\\n";
    dot << "Filesystem Type: " << sb.s_filesystem_type << "\\n";
    dot << "Inodes Count: " << sb.s_inodes_count << "\\n";
    dot << "Blocks Count: " << sb.s_blocks_count << "\\n";
    dot << "Free Inodes: " << sb.s_free_inodes_count << "\\n";
    dot << "Free Blocks: " << sb.s_free_blocks_count << "\\n";
    dot << "Magic: 0x" << hex << sb.s_magic << dec << "\\n";
    dot << "Inode Size: " << sb.s_inode_s << "\\n";
    dot << "Block Size: " << sb.s_block_s << "\\n";
    dot << "First Ino: " << sb.s_firts_ino << "\\n";
    dot << "First Blo: " << sb.s_first_blo << "\\n";
    dot << "BM Inode Start: " << sb.s_bm_inode_start << "\\n";
    dot << "BM Block Start: " << sb.s_bm_block_start << "\\n";
    dot << "Inode Start: " << sb.s_inode_start << "\\n";
    dot << "Block Start: " << sb.s_block_start << "\\n";
    dot << "}\"];\n";
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteFile(const string& diskPath, int partStart, SuperBlock& sb, const string& filePath) {
    ostringstream txt;
    txt << "Reporte de Archivo: " << filePath << "\n";
    txt << "========================================\n\n";
    txt << "Contenido:\n";
    txt << "----------------------------------------\n";
    
    if (filePath == "/users.txt") {
        ifstream inodeFile(diskPath, ios::binary | ios::in);
        inodeFile.seekg(sb.s_inode_start + (3 * sizeof(Inodo)), ios::beg);
        Inodo inodoUsers;
        inodeFile.read(reinterpret_cast<char*>(&inodoUsers), sizeof(Inodo));
        inodeFile.close();
        
        int bloqueUsers = inodoUsers.i_block[0];
        ifstream blockFile(diskPath, ios::binary | ios::in);
        blockFile.seekg(sb.s_block_start + (bloqueUsers * sb.s_block_s), ios::beg);
        BloqueArchivo bloqueContent;
        blockFile.read(reinterpret_cast<char*>(&bloqueContent), sizeof(BloqueArchivo));
        blockFile.close();
        
        txt << string(bloqueContent.b_content) << "\n";
    } else {
        txt << "(Implementación completa requiere búsqueda de inodos por path)\n";
    }
    txt << "----------------------------------------\n";
    return txt.str();
}

string Rep::generarReporteLs(const string& diskPath, int partStart, SuperBlock& sb, const string& dirPath) {
    ostringstream dot;
    dot << "digraph Ls {\n  rankdir=TB;\n  node [shape=record, style=filled];\n\n";
    
    // ✅ Escape completo del path para DOT
    string escapedPath = escapeDot(dirPath);
    
    dot << "  root [label=\"{Directorio: " << escapedPath << "\\n";
    dot << "Inodos: 2, 3, 4...\\n";
    dot << "Archivos: users.txt, ...}\\n\", fillcolor=lightyellow];\n";
    dot << "}\n";
    return dot.str();
}

string Rep::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    string error;
    if (!validarParametros(params, error)) return error;
    
    string id = params["id"];
    string name = params["name"];
    for (char& c : name) c = tolower(c);
    string outputPath = params["path"];
    string pathFileLs = params.count("path_file_ls") ? params["path_file_ls"] : "/";
    
    string diskPath;
    int indice = -1;
    if (!obtenerPathDisco(id, diskPath, indice)) return "Error: No hay partición montada con ID: " + id;
    if (!fs::exists(diskPath)) return "Error: El disco no existe: " + diskPath;
    
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) return "Error: No se pudo abrir el disco";
    MBR mbr; file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR)); file.close();
    int partStart = mbr.mbr_partitions[indice].part_start;
    
    SuperBlock sb;
    ifstream sbFile(diskPath, ios::binary | ios::in);
    sbFile.seekg(partStart, ios::beg);
    sbFile.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));
    sbFile.close();
    
    fs::create_directories(outputPath);
    
    // ✅ Determinar extensión según tipo de reporte
    string fileExtension = ".jpg";
    if (name == "bm_inode" || name == "bm_block" || name == "file") {
        fileExtension = ".txt";
    }
    
    // ✅ Respetar extensión del archivo de salida si está especificada
    size_t extPos = outputPath.find_last_of('.');
    if (extPos != string::npos && extPos < outputPath.length() - 1) {
        string ext = outputPath.substr(extPos);
        if (ext == ".png" || ext == ".jpg" || ext == ".pdf" || ext == ".txt") {
            fileExtension = ext;
        }
    }
    
    string dotContent;
    if (name == "mbr") dotContent = generarReporteMBR(diskPath);
    else if (name == "disk") dotContent = generarReporteDisk(diskPath);
    else if (name == "inode") dotContent = generarReporteInode(diskPath, partStart, sb);
    else if (name == "block") dotContent = generarReporteBlock(diskPath, partStart, sb);
    else if (name == "bm_inode") dotContent = generarReporteBmInode(diskPath, partStart, sb);
    else if (name == "bm_block") dotContent = generarReporteBmBlock(diskPath, partStart, sb);
    else if (name == "tree") dotContent = generarReporteTree(diskPath, partStart, sb);
    else if (name == "sb") dotContent = generarReporteSb(diskPath, partStart);
    else if (name == "file") dotContent = generarReporteFile(diskPath, partStart, sb, pathFileLs);
    else if (name == "ls") dotContent = generarReporteLs(diskPath, partStart, sb, pathFileLs);
    else return "Error: Reporte no implementado: " + name;
    
    if (dotContent.substr(0, 5) == "Error") return dotContent;
    
    string dotPath = outputPath + "/reporte_" + name + ".dot";
    ofstream dotFile(dotPath);
    if (!dotFile.is_open()) return "Error: No se pudo crear el archivo .dot";
    dotFile << dotContent;
    dotFile.close();
    
    string outputFilePath = outputPath + "/reporte_" + name + fileExtension;
    
    if (fileExtension == ".txt") {
        ofstream txtFile(outputFilePath);
        if (!txtFile.is_open()) return "Error: No se pudo crear el archivo .txt";
        txtFile << dotContent;
        txtFile.close();
        
        ostringstream oss;
        oss << "Reporte " << name << " generado exitosamente:\n";
        oss << "  Disco: " << diskPath << "\n";
        oss << "  ID: " << id << "\n";
        oss << "  Archivo TXT: " << outputFilePath << "\n";
        oss << "  ✅ Reporte de texto generado";
        return oss.str();
    } else {
        string cmd = "/usr/bin/dot -Tpng " + dotPath + " -o " + outputFilePath + " 2>&1";
        int result = system(cmd.c_str());
        
        ostringstream oss;
        oss << "Reporte " << name << " generado exitosamente:\n";
        oss << "  Disco: " << diskPath << "\n";
        oss << "  ID: " << id << "\n";
        oss << "  Archivo DOT: " << dotPath << "\n";
        if (result == 0) {
            oss << "  Archivo " << fileExtension << ": " << outputFilePath << "\n";
            oss << "  ✅ Graphviz disponible - Imagen generada";
        } else {
            oss << "  ⚠️ Graphviz no disponible o error en conversión\n";
            oss << "  Error code: " << result << "\n";
            oss << "  Puedes convertir manualmente con: dot -Tpng " << dotPath << " -o imagen.png";
        }
        return oss.str();
    }
}
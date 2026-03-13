#include "rep.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <iomanip>

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

bool Rep::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    if (params.find("name") == params.end()) {
        error = "Error: El parámetro -name es obligatorio";
        return false;
    }
    if (params.find("id") == params.end()) {
        error = "Error: El parámetro -id es obligatorio";
        return false;
    }
    
    string name = params.at("name");
    for (char& c : name) c = tolower(c);
    if (name != "mbr" && name != "disk") {
        error = "Error: El parámetro -name solo acepta: mbr, disk";
        return false;
    }
    
    return true;
}

// ✅ Obtener path del disco desde el ID montado
bool Rep::obtenerPathDisco(const string& id, string& path, int& indice) {
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
            default:
                if (c >= 32 && c <= 126) {
                    resultado += c;
                }
                break;
        }
    }
    return resultado;
}

string Rep::generarReporteMBR(const string& diskPath) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) {
        return "Error: No se pudo abrir el disco: " + diskPath;
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    if (mbr.mbr_tamano <= 0 || mbr.mbr_tamano > 10737418240) {
        ostringstream oss;
        oss << "Error: MBR inválido (tamaño: " << mbr.mbr_tamano << " bytes). ";
        oss << "Verifique que el disco se creó correctamente con MKDISK y FDISK.";
        return oss.str();
    }
    
    ostringstream dot;
    dot << "digraph MBR {\n";
    dot << "  rankdir=TB;\n";
    dot << "  node [shape=record, style=filled, fillcolor=lightblue];\n";
    dot << "  \n";
    
    char fechaStr[50];
    strftime(fechaStr, sizeof(fechaStr), "%Y-%m-%d %H:%M:%S", localtime(&mbr.mbr_fecha_creacion));
    
    dot << "  mbr [label=\"{MBR\\n";
    dot << "Tamaño: " << mbr.mbr_tamano << " bytes\\n";
    dot << "Fecha: " << fechaStr << "\\n";
    dot << "Signature: 0x" << hex << mbr.mbr_dsk_signature << dec << "\\n";
    dot << "Fit: " << mbr.dsk_fit << "}\"];\n";
    dot << "  \n";
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' || mbr.mbr_partitions[i].part_type == 'P' || mbr.mbr_partitions[i].part_type == 'E') {
            string nombre = "";
            for (int j = 0; j < 16 && mbr.mbr_partitions[i].part_name[j] != '\0'; j++) {
                char c = mbr.mbr_partitions[i].part_name[j];
                if (c >= 32 && c <= 126) nombre += c;
            }
            
            string id = "";
            for (int j = 0; j < 4 && mbr.mbr_partitions[i].part_id[j] != '\0'; j++) {
                char c = mbr.mbr_partitions[i].part_id[j];
                if (c >= 32 && c <= 126) id += c;
            }
            
            dot << "  p" << i << " [label=\"{Partición " << i << "\\n";
            dot << "Nombre: " << escapeDot(nombre) << "\\n";
            dot << "Tipo: " << mbr.mbr_partitions[i].part_type << "\\n";
            dot << "Start: " << mbr.mbr_partitions[i].part_start << "\\n";
            dot << "Size: " << mbr.mbr_partitions[i].part_size << " bytes\\n";
            if (!id.empty()) {
                dot << "ID: " << escapeDot(id) << "\\n";
            }
            dot << "Fit: " << mbr.mbr_partitions[i].part_fit << "}\"];\n";
            dot << "  mbr -> p" << i << ";\n";
        }
    }
    
    dot << "}\n";
    return dot.str();
}

string Rep::generarReporteDisk(const string& diskPath) {
    ifstream file(diskPath, ios::binary | ios::in);
    if (!file.is_open()) {
        return "Error: No se pudo abrir el disco";
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    if (mbr.mbr_tamano <= 0) {
        return "Error: MBR inválido";
    }
    
    ostringstream dot;
    dot << "digraph DISK {\n";
    dot << "  rankdir=LR;\n";
    dot << "  node [shape=box, style=filled];\n";
    dot << "  \n";
    
    dot << "  mbr [label=\"MBR\\n(64 bytes)\", fillcolor=lightyellow];\n";
    
    int espacioUsado = sizeof(MBR);
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' || mbr.mbr_partitions[i].part_type == 'P' || mbr.mbr_partitions[i].part_type == 'E') {
            string nombre = "";
            for (int j = 0; j < 16 && mbr.mbr_partitions[i].part_name[j] != '\0'; j++) {
                char c = mbr.mbr_partitions[i].part_name[j];
                if (c >= 32 && c <= 126) nombre += c;
            }
            
            int size = mbr.mbr_partitions[i].part_size;
            int width = min(200, max(20, size / 50000));
            
            dot << "  p" << i << " [label=\"" << escapeDot(nombre) << "\\n" 
                << size << " bytes\", width=" << width/50.0 << ", fillcolor=lightgreen];\n";
            dot << "  mbr -> p" << i << ";\n";
            espacioUsado += size;
        }
    }
    
    int espacioLibre = mbr.mbr_tamano - espacioUsado;
    if (espacioLibre > 0) {
        int width = min(200, max(20, espacioLibre / 50000));
        dot << "  free [label=\"Libre\\n" << espacioLibre << " bytes\", width=" << width/50.0 << ", fillcolor=lightgray];\n";
        dot << "  mbr -> free;\n";
    }
    
    dot << "}\n";
    return dot.str();
}

string Rep::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string id = params["id"];
    string name = params["name"];
    for (char& c : name) c = tolower(c);
    string outputPath = params["path"];
    
    // ✅ Obtener path del disco desde el ID montado
    string diskPath;
    int indice = -1;
    if (!obtenerPathDisco(id, diskPath, indice)) {
        return "Error: No hay una partición montada con ID: " + id;
    }
    
    if (!fs::exists(diskPath)) {
        return "Error: El disco no existe en: " + diskPath;
    }
    
    // Crear directorio de salida
    fs::create_directories(outputPath);
    
    string dotContent;
    string reportType = name;
    
    // Generar contenido según tipo de reporte
    if (name == "mbr") {
        dotContent = generarReporteMBR(diskPath);
    } else if (name == "disk") {
        dotContent = generarReporteDisk(diskPath);
    }
    
    // Verificar si hay error
    if (dotContent.substr(0, 5) == "Error") {
        return dotContent;
    }
    
    // Guardar archivo .dot
    string dotPath = outputPath + "/reporte_" + name + ".dot";
    ofstream dotFile(dotPath);
    if (!dotFile.is_open()) {
        return "Error: No se pudo crear el archivo .dot";
    }
    dotFile << dotContent;
    dotFile.close();
    
    // Convertir a PNG con graphviz
    string pngPath = outputPath + "/reporte_" + name + ".png";
    string cmd = "/usr/bin/dot -Tpng " + dotPath + " -o " + pngPath + " 2>&1";
    int result = system(cmd.c_str());
    
    ostringstream oss;
    oss << "Reporte " << reportType << " generado exitosamente:\n";
    oss << "  Disco: " << diskPath << "\n";
    oss << "  ID: " << id << "\n";
    oss << "  Archivo DOT: " << dotPath << "\n";
    if (result == 0) {
        oss << "  Archivo PNG: " << pngPath << "\n";
        oss << "  ✅ Graphviz disponible - Imagen generada";
    } else {
        oss << "  ⚠️ Graphviz no disponible o error en conversión\n";
        oss << "  Error code: " << result << "\n";
        oss << "  Puedes convertir manualmente con: dot -Tpng " << dotPath << " -o imagen.png";
    }
    
    return oss.str();
}
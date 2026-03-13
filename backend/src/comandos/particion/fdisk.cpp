#include "fdisk.h"
#include "../../structs.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

map<string, string> FDisk::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "fdisk"
    
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

bool FDisk::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("size") == params.end()) {
        error = "Error: El parámetro -size es obligatorio";
        return false;
    }
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    if (params.find("name") == params.end()) {
        error = "Error: El parámetro -name es obligatorio";
        return false;
    }
    
    // Validar size > 0
    try {
        long long size = stoll(params.at("size"));
        if (size <= 0) {
            error = "Error: El tamaño debe ser mayor a cero";
            return false;
        }
    } catch (...) {
        error = "Error: El parámetro -size debe ser un número válido";
        return false;
    }
    
    // Validar unit (default: K)
    if (params.count("unit")) {
        char unit = toupper(params.at("unit")[0]);
        if (unit != 'B' && unit != 'K' && unit != 'M') {
            error = "Error: El parámetro -unit solo acepta B, K o M";
            return false;
        }
    }
    
    // Validar type (default: P)
    if (params.count("type")) {
        char type = toupper(params.at("type")[0]);
        if (type != 'P' && type != 'E' && type != 'L') {
            error = "Error: El parámetro -type solo acepta P, E o L";
            return false;
        }
    }
    
    // Validar fit (default: WF)
    if (params.count("fit")) {
        string fit = params.at("fit");
        for (char& c : fit) c = toupper(c);
        if (fit != "BF" && fit != "FF" && fit != "WF") {
            error = "Error: El parámetro -fit solo acepta BF, FF o WF";
            return false;
        }
    }
    
    return true;
}

long long FDisk::calcularTamañoBytes(long long size, char unit) {
    switch (toupper(unit)) {
        case 'B': return size;
        case 'K': return size * 1024;
        case 'M': return size * 1024 * 1024;
        default: return size * 1024;  // Default: Kilobytes
    }
}

bool FDisk::leerMBR(const string& path, MBR& mbr) {
    ifstream file(path, ios::binary | ios::in);
    if (!file.is_open()) {
        return false;
    }
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    return true;
}

bool FDisk::escribirMBR(const string& path, const MBR& mbr) {
    fstream file(path, ios::binary | ios::in | ios::out);
    if (!file.is_open()) {
        return false;
    }
    file.seekp(0, ios::beg);
    file.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    file.close();
    return true;
}

int FDisk::buscarEspacioLibre(const MBR& mbr, long long tamaño, char fit) {
    int mejorIndice = -1;
    int espacioLibre = INT32_MAX;
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '0' || mbr.mbr_partitions[i].part_type == '\0') {
            // partición disponible
            if (fit == 'F') {
                return i;  // First Fit
            } else if (fit == 'B') {
                // Best Fit - buscar el espacio más pequeño que quepa
                if (mbr.mbr_partitions[i].part_size < espacioLibre) {
                    espacioLibre = mbr.mbr_partitions[i].part_size;
                    mejorIndice = i;
                }
            } else if (fit == 'W') {
                // Worst Fit - buscar el espacio más grande
                if (mbr.mbr_partitions[i].part_size > espacioLibre) {
                    espacioLibre = mbr.mbr_partitions[i].part_size;
                    mejorIndice = i;
                }
            }
        }
    }
    
    return (fit == 'B' || fit == 'W') ? mejorIndice : -1;
}

bool FDisk::crearParticionPrimaria(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes) {
    // Verificar si ya existe una partición con el mismo nombre
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            if (string(mbr.mbr_partitions[i].part_name) == params.at("name")) {
                return false;  // Nombre duplicado
            }
        }
    }
    
    // Contar particiones primarias y extendidas
    int primariasExtendidas = 0;
    bool tieneExtendida = false;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            primariasExtendidas++;
            if (mbr.mbr_partitions[i].part_type == 'E') {
                tieneExtendida = true;
            }
        }
    }
    
    if (primariasExtendidas >= 4) {
        return false;  // Máximo 4 particiones
    }
    
    // Buscar espacio disponible
    int indice = -1;
    char fit = params.count("fit") ? toupper(params.at("fit")[0]) : 'W';
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '0' || mbr.mbr_partitions[i].part_type == '\0') {
            indice = i;
            if (fit == 'F') break;
        }
    }
    
    if (indice == -1) {
        return false;  // No hay espacio
    }
    
    // Calcular start de la partición (después del MBR)
    int start = sizeof(MBR);
    if (indice > 0) {
        // Buscar el end de la partición anterior
        for (int i = 0; i < indice; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                start = mbr.mbr_partitions[i].part_start + mbr.mbr_partitions[i].part_size;
            }
        }
    }
    
    // Verificar que hay espacio suficiente
    if (start + tamañoBytes > mbr.mbr_tamano) {
        return false;
    }
    
    // Crear la partición
    mbr.mbr_partitions[indice].part_status = '1';
    mbr.mbr_partitions[indice].part_type = 'P';
    mbr.mbr_partitions[indice].part_fit = fit;
    mbr.mbr_partitions[indice].part_start = start;
    mbr.mbr_partitions[indice].part_size = static_cast<int32_t>(tamañoBytes);
    strncpy(mbr.mbr_partitions[indice].part_name, params.at("name").c_str(), 16);
    mbr.mbr_partitions[indice].part_correlative = -1;
    memset(mbr.mbr_partitions[indice].part_id, 0, 4);
    
    return escribirMBR(path, mbr);
}

bool FDisk::crearParticionExtendida(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes) {
    // Verificar si ya existe una partición extendida
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' && mbr.mbr_partitions[i].part_type == 'E') {
            return false;  // Solo una extendida por disco
        }
    }
    
    // Verificar si ya existe una partición con el mismo nombre
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            if (string(mbr.mbr_partitions[i].part_name) == params.at("name")) {
                return false;
            }
        }
    }
    
    // Contar particiones primarias y extendidas
    int primariasExtendidas = 0;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            primariasExtendidas++;
        }
    }
    
    if (primariasExtendidas >= 4) {
        return false;
    }
    
    // Buscar espacio disponible
    int indice = -1;
    char fit = params.count("fit") ? toupper(params.at("fit")[0]) : 'W';
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '0' || mbr.mbr_partitions[i].part_type == '\0') {
            indice = i;
            if (fit == 'F') break;
        }
    }
    
    if (indice == -1) {
        return false;
    }
    
    // Calcular start
    int start = sizeof(MBR);
    if (indice > 0) {
        for (int i = 0; i < indice; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                start = mbr.mbr_partitions[i].part_start + mbr.mbr_partitions[i].part_size;
            }
        }
    }
    
    if (start + tamañoBytes > mbr.mbr_tamano) {
        return false;
    }
    
    // Crear la partición extendida
    mbr.mbr_partitions[indice].part_status = '1';
    mbr.mbr_partitions[indice].part_type = 'E';
    mbr.mbr_partitions[indice].part_fit = fit;
    mbr.mbr_partitions[indice].part_start = start;
    mbr.mbr_partitions[indice].part_size = static_cast<int32_t>(tamañoBytes);
    strncpy(mbr.mbr_partitions[indice].part_name, params.at("name").c_str(), 16);
    mbr.mbr_partitions[indice].part_correlative = -1;
    memset(mbr.mbr_partitions[indice].part_id, 0, 4);
    
    // Crear el primer EBR al inicio de la partición extendida
    EBR ebr;
    memset(&ebr, 0, sizeof(EBR));
    ebr.part_status = '0';
    ebr.part_fit = fit;
    ebr.part_start = -1;
    ebr.part_size = 0;
    ebr.part_next = -1;
    memset(ebr.part_name, 0, 16);
    
    fstream file(path, ios::binary | ios::in | ios::out);
    if (!file.is_open()) {
        return false;
    }
    file.seekp(start, ios::beg);
    file.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
    file.close();
    
    return escribirMBR(path, mbr);
}

bool FDisk::crearParticionLogica(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes) {
    // Buscar la partición extendida
    int indiceExtendida = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' && mbr.mbr_partitions[i].part_type == 'E') {
            indiceExtendida = i;
            break;
        }
    }
    
    if (indiceExtendida == -1) {
        return false;  // No hay partición extendida
    }
    
    // Leer el primer EBR
    int ebrStart = mbr.mbr_partitions[indiceExtendida].part_start;
    EBR ebr;
    
    // ✅ Usar ifstream solo para leer
    ifstream fileRead(path, ios::binary | ios::in);
    if (!fileRead.is_open()) {
        return false;
    }
    fileRead.seekg(ebrStart, ios::beg);
    fileRead.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    fileRead.close();
    
    // Buscar el último EBR en la cadena
    int ultimoEBR = ebrStart;
    while (ebr.part_next != -1) {
        ultimoEBR = ebr.part_next;
        
        // ✅ Usar ifstream para leer el siguiente EBR
        ifstream fileNext(path, ios::binary | ios::in);
        if (!fileNext.is_open()) {
            return false;
        }
        fileNext.seekg(ultimoEBR, ios::beg);
        fileNext.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        fileNext.close();
    }
    
    // Calcular start para la nueva partición lógica
    int start = ultimoEBR + sizeof(EBR);
    int endExtendida = mbr.mbr_partitions[indiceExtendida].part_start + mbr.mbr_partitions[indiceExtendida].part_size;
    
    if (start + tamañoBytes > endExtendida) {
        return false;  // No hay espacio en la extendida
    }
    
    // Crear nuevo EBR para la partición lógica
    EBR nuevoEBR;
    memset(&nuevoEBR, 0, sizeof(EBR));
    nuevoEBR.part_status = '1';
    nuevoEBR.part_fit = params.count("fit") ? toupper(params.at("fit")[0]) : 'W';
    nuevoEBR.part_start = start;
    nuevoEBR.part_size = static_cast<int32_t>(tamañoBytes);
    nuevoEBR.part_next = -1;
    strncpy(nuevoEBR.part_name, params.at("name").c_str(), 16);
    
    // ✅ Usar fstream para ESCRIBIR el nuevo EBR
    fstream fileWrite(path, ios::binary | ios::in | ios::out);
    if (!fileWrite.is_open()) {
        return false;
    }
    fileWrite.seekp(ultimoEBR, ios::beg);  // ✅ seekp funciona con fstream
    fileWrite.write(reinterpret_cast<const char*>(&nuevoEBR), sizeof(EBR));  // ✅ write funciona con fstream
    fileWrite.close();
    
    return true;
}

string FDisk::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string path = params["path"];
    
    // Verificar que el disco existe
    if (!fs::exists(path)) {
        return "Error: El disco no existe en: " + path;
    }
    
    // Leer MBR
    MBR mbr;
    if (!leerMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR del disco";
    }
    
    // Calcular tamaño en bytes
    long long size = stoll(params["size"]);
    char unit = params.count("unit") ? toupper(params["unit"][0]) : 'K';
    long long tamañoBytes = calcularTamañoBytes(size, unit);
    
    // Obtener tipo de partición
    char type = params.count("type") ? toupper(params["type"][0]) : 'P';
    
    bool exito = false;
    switch (type) {
        case 'P':
            exito = crearParticionPrimaria(path, mbr, params, tamañoBytes);
            break;
        case 'E':
            exito = crearParticionExtendida(path, mbr, params, tamañoBytes);
            break;
        case 'L':
            exito = crearParticionLogica(path, mbr, params, tamañoBytes);
            break;
    }
    
    if (exito) {
        ostringstream oss;
        oss << "Partición creada exitosamente:\n";
        oss << "  Nombre: " << params["name"] << "\n";
        oss << "  Tipo: " << type << "\n";
        oss << "  Tamaño: " << size << " " << unit << " (" << tamañoBytes << " bytes)";
        return oss.str();
    } else {
        return "Error: No se pudo crear la partición. Verifique espacio disponible o restricciones.";
    }
}

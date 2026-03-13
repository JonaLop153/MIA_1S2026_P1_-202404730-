#include "mkdisk.h"
#include "../../structs.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <random>

namespace fs = std::filesystem;

// Parsear parámetros: "mkdisk -size=10 -unit=M -path=/home/disco.mia"
map<string, string> MKDisk::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    // Ignorar el nombre del comando
    iss >> token;
    
    // Parsear cada parámetro -clave=valor
    while (iss >> token) {
        if (token[0] == '-') {
            size_t eqPos = token.find('=');
            if (eqPos != string::npos) {
                string key = token.substr(1, eqPos - 1);  // Sin el '-'
                string value = token.substr(eqPos + 1);
                
                // Remover comillas si existen
                if (!value.empty() && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                // Convertir key a minúsculas para comparación case-insensitive
                for (char& c : key) c = tolower(c);
                
                params[key] = value;
            }
        }
    }
    return params;
}

// Validar parámetros obligatorios y valores permitidos
bool MKDisk::validarParametros(const map<string, string>& params, string& error) {
    // Verificar parámetros obligatorios
    if (params.find("size") == params.end()) {
        error = "Error: El parámetro -size es obligatorio";
        return false;
    }
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
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
    
    // Validar unit (si existe)
    if (params.count("unit")) {
        char unit = toupper(params.at("unit")[0]);
        if (unit != 'K' && unit != 'M') {
            error = "Error: El parámetro -unit solo acepta K o M";
            return false;
        }
    }
    
    // Validar fit (si existe)
    if (params.count("fit")) {
        string fit = params.at("fit");
        // Convertir a mayúsculas para comparar
        for (char& c : fit) c = toupper(c);
        if (fit != "BF" && fit != "FF" && fit != "WF") {
            error = "Error: El parámetro -fit solo acepta BF, FF o WF";
            return false;
        }
    }
    
    return true;
}

// Calcular tamaño en bytes: K=1024, M=1024*1024, B=1
long long MKDisk::calcularTamañoBytes(long long size, char unit) {
    switch (toupper(unit)) {
        case 'K': return size * 1024;
        case 'M': return size * 1024 * 1024;
        case 'B': return size;
        default: return size * 1024 * 1024;  // Default: Megabytes
    }
}

// Crear archivo binario lleno de ceros
bool MKDisk::crearArchivoBinario(const string& path, long long tamaño) {
    // Crear directorios padres si no existen
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    
    ofstream file(path, ios::binary | ios::out);
    if (!file.is_open()) {
        return false;
    }
    
    // Usar buffer de 1024 bytes para escribir más rápido
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE] = {0};  // Inicializado en ceros
    
    long long restantes = tamaño;
    while (restantes > 0) {
        int escribir = (restantes >= BUFFER_SIZE) ? BUFFER_SIZE : restantes;
        file.write(buffer, escribir);
        restantes -= escribir;
    }
    
    file.close();
    return true;
}

// Escribir la estructura MBR al inicio del archivo
bool MKDisk::escribirMBR(const string& path, const map<string, string>& params) {
    // ✅ Usar fstream en lugar de ifstream para poder escribir
    fstream file(path, ios::binary | ios::in | ios::out);
    if (!file.is_open()) {
        return false;
    }
    
    // Preparar estructura MBR
    MBR mbr;
    memset(&mbr, 0, sizeof(MBR));  // Inicializar en ceros
    
    // Calcular tamaño total del disco en bytes
    long long size = stoll(params.at("size"));
    char unit = params.count("unit") ? toupper(params.at("unit")[0]) : 'M';
    mbr.mbr_tamano = static_cast<int32_t>(calcularTamañoBytes(size, unit));
    
    // Fecha de creación
    mbr.mbr_fecha_creacion = time(nullptr);
    
    // Signature aleatoria única
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(10000, 99999);
    mbr.mbr_dsk_signature = dis(gen);
    
    // Ajuste del disco (default: FF)
    string fit = params.count("fit") ? params.at("fit") : "FF";
    for (char& c : fit) c = toupper(c);
    mbr.dsk_fit = fit[0];  // 'B', 'F' o 'W'
    
    // Inicializar las 4 particiones en ceros/desactivadas
    for (int i = 0; i < 4; i++) {
        mbr.mbr_partitions[i].part_status = '0';  // Desmontada
        mbr.mbr_partitions[i].part_type = '\0';
        mbr.mbr_partitions[i].part_fit = '\0';
        mbr.mbr_partitions[i].part_start = -1;
        mbr.mbr_partitions[i].part_size = 0;
        memset(mbr.mbr_partitions[i].part_name, 0, 16);
        mbr.mbr_partitions[i].part_correlative = -1;
        memset(mbr.mbr_partitions[i].part_id, 0, 4);
    }
    
    // ✅ Escribir MBR al inicio del archivo (byte 0)
    file.seekp(0, ios::beg);  // ✅ seekp funciona con fstream
    file.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));  // ✅ write funciona con fstream
    
    file.close();
    return true;
}

// Función principal que ejecuta el comando
string MKDisk::ejecutar(const string& comando) {
    // 1. Parsear parámetros
    map<string, string> params = parsearParametros(comando);
    
    // 2. Validar parámetros
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    // 3. Obtener valores
    long long size = stoll(params["size"]);
    char unit = params.count("unit") ? toupper(params["unit"][0]) : 'M';
    string path = params["path"];
    string fit = params.count("fit") ? params["fit"] : "FF";
    for (char& c : fit) c = toupper(c);
    
    // 4. Calcular tamaño en bytes
    long long tamañoBytes = calcularTamañoBytes(size, unit);
    
    // 5. Crear archivo binario con ceros
    if (!crearArchivoBinario(path, tamañoBytes)) {
        return "Error: No se pudo crear el archivo en: " + path;
    }
    
    // 6. Escribir MBR al inicio
    if (!escribirMBR(path, params)) {
        return "Error: No se pudo escribir el MBR en: " + path;
    }
    
    // 7. Éxito
    ostringstream oss;
    oss << "Disco creado exitosamente:\n";
    oss << "  Path: " << path << "\n";
    oss << "  Tamaño: " << size << " " << unit << " (" << tamañoBytes << " bytes)\n";
    oss << "  Fit: " << fit << "\n";
    oss << "  MBR escrito en byte 0";
    
    return oss.str();
}

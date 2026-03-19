#include "mkdisk.h"
#include "../../structs.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <random>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

map<string, string> MKDisk::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "mkdisk"
    
    // ✅ Parámetros válidos para MKDISK
    vector<string> validos = {"size", "fit", "unit", "path"};
    
    while (iss >> token) {
        if (token[0] == '-') {
            size_t eqPos = token.find('=');
            if (eqPos != string::npos) {
                string key = token.substr(1, eqPos - 1);
                string value = token.substr(eqPos + 1);
                
                // Remover comillas si existen
                if (!value.empty() && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                // Convertir key a minúsculas
                for (char& c : key) c = tolower(c);
                
                // ✅ Validar que el parámetro sea reconocido
                if (find(validos.begin(), validos.end(), key) == validos.end()) {
                    params["__error_param__"] = key;
                }
                
                params[key] = value;
            }
        }
    }
    return params;
}

bool MKDisk::validarParametros(const map<string, string>& params, string& error) {
    // ✅ Verificar parámetro no reconocido
    if (params.count("__error_param__")) {
        error = "Error: Parámetro no reconocido: -" + params.at("__error_param__");
        return false;
    }
    
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
        int size = stoi(params.at("size"));
        if (size <= 0) {
            error = "Error: El tamaño debe ser mayor a cero";
            return false;
        }
    } catch (...) {
        error = "Error: El parámetro -size debe ser un número válido";
        return false;
    }
    
    // Validar fit
    if (params.count("fit")) {
        string fit = params.at("fit");
        for (char& c : fit) c = toupper(c);
        if (fit != "BF" && fit != "FF" && fit != "WF") {
            error = "Error: El parámetro -fit solo acepta BF, FF o WF";
            return false;
        }
    }
    
    // Validar unit
    if (params.count("unit")) {
        string unit = params.at("unit");
        for (char& c : unit) c = toupper(c);
        if (unit != "K" && unit != "M") {
            error = "Error: El parámetro -unit solo acepta K o M";
            return false;
        }
    }
    
    return true;
}

string MKDisk::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    int size = stoi(params["size"]);
    string unit = params.count("unit") ? params["unit"] : "M";
    string fit = params.count("fit") ? params["fit"] : "FF";
    string path = params["path"];
    
    // Convertir a mayúsculas
    for (char& c : unit) c = toupper(c);
    for (char& c : fit) c = toupper(c);
    
    // Calcular tamaño en bytes
    int sizeBytes = size;
    if (unit == "K") sizeBytes *= 1024;
    else if (unit == "M") sizeBytes *= 1024 * 1024;
    
    // Crear directorio padre si no existe
    fs::path fsPath(path);
    if (fsPath.has_parent_path()) {
        fs::create_directories(fsPath.parent_path());
    }
    
    // ✅ Crear archivo binario lleno de ceros
    ofstream file(path, ios::binary | ios::out);
    if (!file.is_open()) {
        return "Error: No se pudo crear el disco en: " + path;
    }
    
    // Usar buffer de 1024 bytes para eficiencia (según enunciado)
    const int BUFFER_SIZE = 1024;
    char* buffer = new char[BUFFER_SIZE]();  // Inicializado en 0
    
    int remaining = sizeBytes;
    while (remaining > 0) {
        int toWrite = min(BUFFER_SIZE, remaining);
        file.write(buffer, toWrite);
        remaining -= toWrite;
    }
    
    delete[] buffer;
    file.close();
    
    // ✅ ESCRIBIR MBR AL INICIO DEL DISCO (primer sector)
    fstream mbrFile(path, ios::binary | ios::in | ios::out);
    if (!mbrFile.is_open()) {
        return "Error: No se pudo abrir el disco para escribir MBR";
    }
    
    // ✅ Crear estructura MBR según enunciado
    MBR mbr;
    memset(&mbr, 0, sizeof(MBR));  // Limpiar toda la estructura
    
    // Campos del MBR según PDF (página 10-11)
    mbr.mbr_tamano = sizeBytes;              // Tamaño total del disco en bytes
    mbr.mbr_fecha_creacion = time(nullptr);  // Fecha y hora de creación
    
    // Generar signature random (número único para cada disco)
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1000, 99999);
    mbr.mbr_dsk_signature = dis(gen);
    
    mbr.dsk_fit = fit[0];  // Tipo de ajuste (B, F, W)
    
    // ✅ Inicializar las 4 particiones del MBR
    for (int i = 0; i < 4; i++) {
        mbr.mbr_partitions[i].part_status = '0';      // '0' = no montada, '1' = montada
        mbr.mbr_partitions[i].part_type = '\0';       // '\0' = vacía, 'P' = primaria, 'E' = extendida
        mbr.mbr_partitions[i].part_fit = '\0';        // Ajuste de partición
        mbr.mbr_partitions[i].part_start = -1;        // Byte de inicio (-1 = no asignado)
        mbr.mbr_partitions[i].part_size = -1;         // Tamaño en bytes (-1 = no asignado)
        memset(mbr.mbr_partitions[i].part_name, 0, 16);  // Nombre vacío
        mbr.mbr_partitions[i].part_correlative = -1;  // -1 hasta que sea montada
        memset(mbr.mbr_partitions[i].part_id, 0, 4);  // ID vacío hasta mount
    }
    
    // ✅ Escribir MBR en el primer sector del disco (byte 0)
    mbrFile.seekp(0, ios::beg);
    mbrFile.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    mbrFile.close();
    
    // ✅ Respuesta exitosa
    ostringstream oss;
    oss << "Disco creado exitosamente:\n";
    oss << "  Path: " << path << "\n";
    oss << "  Tamaño: " << size << " " << unit << " (" << sizeBytes << " bytes)\n";
    oss << "  Fit: " << fit << "\n";
    oss << "  MBR escrito en byte 0";
    
    return oss.str();
}
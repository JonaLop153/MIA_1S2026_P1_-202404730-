#include "mount.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <random>

namespace fs = std::filesystem;

// Carnet: 202404730 → Últimos 2 dígitos: 30
const string CARNET_DIGITOS = "30";

map<string, string> Mount::parsearParametros(const string& comando) {
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

bool Mount::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("path") == params.end()) {
        error = "Error: El parámetro -path es obligatorio";
        return false;
    }
    if (params.find("name") == params.end()) {
        error = "Error: El parámetro -name es obligatorio";
        return false;
    }
    return true;
}

// Generar ID según formato: Carnet + NúmeroPartición + Letra
// Ej: 301A, 301B, 301C, 302A, 303A
string Mount::generarID(const string& path) {
    static map<string, int> numeroPorDisco;
    static map<string, char> letraPorDisco;
    
    // Si es el primer montaje en este disco
    if (numeroPorDisco.find(path) == numeroPorDisco.end()) {
        numeroPorDisco[path] = 1;
        letraPorDisco[path] = 'A';
    } else {
        // Contar cuántas particiones montadas hay en este disco
        int count = 0;
        MountMap& montadas = getParticionesMontadas();
        for (auto const& [id, data] : montadas) {
            if (data.first == path) {
                count++;
            }
        }
        
        if (count >= 3) {
            // Más de 3 particiones en mismo disco → siguiente letra, reiniciar número
            numeroPorDisco[path] = 1;
            letraPorDisco[path] = letraPorDisco[path] + 1;
        } else {
            // Misma letra, siguiente número
            numeroPorDisco[path] = count + 1;
        }
    }
    
    string id = CARNET_DIGITOS + to_string(numeroPorDisco[path]) + letraPorDisco[path];
    return id;
}

bool Mount::buscarParticion(const string& path, const string& name, char type, int& indice, char& partType) {
    ifstream file(path, ios::binary | ios::in);
    if (!file.is_open()) {
        return false;
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    // Buscar en particiones primarias
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            string partName(mbr.mbr_partitions[i].part_name);
            if (partName == name) {
                if (type == '\0' || mbr.mbr_partitions[i].part_type == type) {
                    indice = i;
                    partType = mbr.mbr_partitions[i].part_type;
                    return true;
                }
            }
        }
    }
    
    return false;
}

string Mount::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string path = params["path"];
    string name = params["name"];
    char type = params.count("type") ? toupper(params["type"][0]) : '\0';
    
    if (!fs::exists(path)) {
        return "Error: El disco no existe en: " + path;
    }
    
    int indice = -1;
    char partType = '\0';
    if (!buscarParticion(path, name, type, indice, partType)) {
        return "Error: No se encontró la partición '" + name + "' en el disco";
    }
    
    // Verificar si ya está montada
    if (isMounted(path, indice)) {
        return "Error: La partición ya está montada";
    }
    
    // Generar ID según formato Carnet
    string id = generarID(path);
    
    // Guardar en mapa de montadas
    mountPartition(id, path, indice);
    
    // Actualizar MBR: part_status = '1' y part_id
    fstream file(path, ios::binary | ios::in | ios::out);
    if (!file.is_open()) {
        return "Error: No se pudo abrir el disco para montar";
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    
    if (indice >= 0 && indice < 4) {
        mbr.mbr_partitions[indice].part_status = '1';
        memset(mbr.mbr_partitions[indice].part_id, 0, 4);
        strncpy(mbr.mbr_partitions[indice].part_id, id.c_str(), 4);
        // part_correlative se actualiza según orden de montaje
        mbr.mbr_partitions[indice].part_correlative = 1;  // Simplificado
    }
    
    file.seekp(0, ios::beg);
    file.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    file.close();
    
    ostringstream oss;
    oss << "Partición montada exitosamente:\n";
    oss << "  Nombre: " << name << "\n";
    oss << "  ID: " << id << "\n";
    oss << "  Path: " << path << "\n";
    oss << "  Tipo: " << partType;
    
    return oss.str();
}
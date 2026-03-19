#include "mount.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <random>
#include <set>       // ← ¡ESTO FALTA!
#include <algorithm> // ← Para distance()

namespace fs = std::filesystem;

// Carnet: 202404730 → Últimos 2 dígitos: 30
const string CARNET_DIGITOS = "30";

map<string, string> Mount::parsearParametros(const string& comando) {
    map<string, string> params;
    istringstream iss(comando);
    string token;
    
    iss >> token;  // Ignorar "mount"
    
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

// ✅ Generar ID según formato Carnet: 30 + número + letra
// Reglas según enunciado:
// - Mismo disco: incrementa número (301A, 302A, 303A, 304A)
// - Otro disco: siguiente letra, reinicia número (301B, 302B, 303B)
string Mount::generarID(const string& path) {
    MountMap& montadas = getParticionesMontadas();
    
    // Contar cuántas particiones de CADA disco están montadas
    map<string, int> countPorDisco;
    
    for (auto const& [id, data] : montadas) {
        countPorDisco[data.first]++;
    }
    
    int countEsteDisco = countPorDisco[path] + 1;
    
    // Calcular letra según el orden del disco
    // Primer disco montado = A, segundo = B, tercero = C, etc.
    set<string> discosUnicos;
    for (auto const& [id, data] : montadas) {
        discosUnicos.insert(data.first);
    }
    
    // Encontrar el índice de este disco en el set ordenado
    char letra = 'A';
    int indice = 0;
    for (const auto& disco : discosUnicos) {
        if (disco == path) {
            letra = 'A' + indice;
            break;
        }
        indice++;
    }
    
    // Si este disco aún no está en el set, usar la siguiente letra
    if (discosUnicos.find(path) == discosUnicos.end()) {
        letra = 'A' + discosUnicos.size();
    }
    
    // Si hay más de 3 particiones en mismo disco, cambiar letra
    if (countEsteDisco > 3) {
        letra = letra + (countEsteDisco - 1) / 3;
        countEsteDisco = ((countEsteDisco - 1) % 3) + 1;
    }
    
    return CARNET_DIGITOS + to_string(countEsteDisco) + letra;
}

// ✅ Buscar partición por nombre en el MBR
bool Mount::buscarParticion(const string& path, const string& name, char type, int& indice, char& partType) {
    ifstream file(path, ios::binary | ios::in);
    if (!file.is_open()) {
        return false;
    }
    
    MBR mbr;
    file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    file.close();
    
    // Buscar en particiones primarias/extendidas
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1' || 
            mbr.mbr_partitions[i].part_type == 'P' || 
            mbr.mbr_partitions[i].part_type == 'E') {
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
    
    // Verificar que el disco existe
    if (!fs::exists(path)) {
        return "Error: El disco no existe en: " + path;
    }
    
    // Buscar la partición
    int indice = -1;
    char partType = '\0';
    if (!buscarParticion(path, name, type, indice, partType)) {
        return "Error: No se encontró la partición '" + name + "' en el disco";
    }
    
    // ✅ VERIFICAR SI YA ESTÁ MONTADA EN RAM (no en MBR)
    if (isMounted(path, indice)) {
        return "Error: La partición ya está montada";
    }
    
    // Generar ID según formato Carnet
    string id = generarID(path);
    
    // Guardar en mapa de montadas (RAM)
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
        mbr.mbr_partitions[indice].part_correlative++;
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
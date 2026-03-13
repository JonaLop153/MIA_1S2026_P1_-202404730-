#include "unmount.h"
#include "../../structs.h"
#include "../../session/session.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

map<string, string> Unmount::parsearParametros(const string& comando) {
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

bool Unmount::validarParametros(const map<string, string>& params, string& error) {
    if (params.find("id") == params.end()) {
        error = "Error: El parámetro -id es obligatorio";
        return false;
    }
    return true;
}

string Unmount::ejecutar(const string& comando) {
    map<string, string> params = parsearParametros(comando);
    
    string error;
    if (!validarParametros(params, error)) {
        return error;
    }
    
    string id = params["id"];
    
    MountMap& montadas = getParticionesMontadas();
    
    if (montadas.find(id) == montadas.end()) {
        return "Error: No hay una partición montada con ID: " + id;
    }
    
    auto& data = montadas.at(id);
    string path = data.first;
    int indice = data.second;
    
    // Leer MBR
    ifstream fileRead(path, ios::binary | ios::in);
    if (!fileRead.is_open()) {
        return "Error: No se pudo abrir el disco";
    }
    
    MBR mbr;
    fileRead.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    fileRead.close();
    
    // ✅ CORRECCIÓN: Solo limpiar el ID, NO cambiar part_status
    if (indice >= 0 && indice < 4) {
        // mbr.mbr_partitions[indice].part_status = '0';  // ❌ ¡NO HACER ESTO!
        memset(mbr.mbr_partitions[indice].part_id, 0, 4);  // ✅ Solo limpiar ID
        
        fstream fileWrite(path, ios::binary | ios::in | ios::out);
        if (!fileWrite.is_open()) {
            return "Error: No se pudo abrir el disco para escritura";
        }
        fileWrite.seekp(0, ios::beg);
        fileWrite.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
        fileWrite.close();
    }
    
    unmountPartition(id);
    
    ostringstream oss;
    oss << "Partición desmontada exitosamente:\n";
    oss << "  ID: " << id << "\n";
    oss << "  Path: " << path << "\n";
    oss << "  Nota: La partición sigue existiendo (puede volver a montarse)";
    
    return oss.str();
}
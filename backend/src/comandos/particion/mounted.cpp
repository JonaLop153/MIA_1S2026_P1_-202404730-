#include "mounted.h"
#include "../../session/session.h"  // ← ¡Usar session.h en lugar de extern!
#include <sstream>
#include <map>
#include <string>
#include <utility>

using namespace std;

string Mounted::ejecutar(const string& comando) {
    MountMap& montadas = getParticionesMontadas();
    
    if (montadas.empty()) {
        return "No hay particiones montadas";
    }
    
    ostringstream oss;
    oss << "Particiones montadas:\n";
    oss << "----------------------------------------\n";
    oss << "ID\tPath\t\tNombre\n";
    oss << "----------------------------------------\n";
    
    for (auto const& [id, data] : montadas) {
        oss << id << "\t" << data.first << "\tParticion\n";
    }
    
    return oss.str();
}
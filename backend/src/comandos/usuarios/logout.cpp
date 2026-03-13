#include "logout.h"
#include "../../session/session.h"
#include <sstream>

string Logout::ejecutar(const string& comando) {
    if (!haySesionActiva()) {
        return "Error: No hay una sesión activa";
    }
    
    string usuario = getUsuarioActual();
    
    // Cerrar sesión
    cerrarSesion();
    
    ostringstream oss;
    oss << "Sesión cerrada exitosamente:\n";
    oss << "  Usuario: " << usuario;
    
    return oss.str();
}
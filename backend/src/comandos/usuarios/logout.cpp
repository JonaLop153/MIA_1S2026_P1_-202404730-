#include "logout.h"
#include "../../session/session.h"
#include <sstream>

string Logout::ejecutar(const string& comando) {
    if (!haySesionActiva()) return "Error: No hay sesión activa";
    
    string usuario = getUsuarioActual();
    cerrarSesion();
    
    ostringstream oss;
    oss << "Sesión cerrada exitosamente:\n";
    oss << "  Usuario: " << usuario;
    return oss.str();
}
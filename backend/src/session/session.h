#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <map>
#include <utility>

using namespace std;

// Estructura para sesión de usuario activo
struct SesionActiva {
    string usuario;
    string password;
    string idParticion;
    string pathDisco;
    int indiceParticion;
    bool activa;
};

// Tipo para el mapa de particiones montadas: ID -> (path, indice)
typedef map<string, pair<string, int>> MountMap;

// Obtener referencia al mapa global de particiones montadas
MountMap& getParticionesMontadas();

// Obtener referencia a la sesión activa
SesionActiva& getSesionActiva();

// Funciones de utilidad
void mountPartition(const string& id, const string& path, int indice);
void unmountPartition(const string& id);
bool isMounted(const string& path, int indice);
string getMountID(const string& path, int indice);

// Funciones de sesión
void iniciarSesion(const string& user, const string& pass, const string& id);
void cerrarSesion();
bool haySesionActiva();
string getUsuarioActual();
string getIdParticionActual();

#endif
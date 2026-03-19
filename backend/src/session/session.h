#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <map>
#include <utility>

using namespace std;

struct SesionActiva {
    string usuario;
    string password;
    string idParticion;
    string pathDisco;
    int indiceParticion;
    bool activa;
};

typedef map<string, pair<string, int>> MountMap;

MountMap& getParticionesMontadas();
SesionActiva& getSesionActiva();

void mountPartition(const string& id, const string& path, int indice);
void unmountPartition(const string& id);
bool isMounted(const string& path, int indice);
string getMountID(const string& path, int indice);

void iniciarSesion(const string& user, const string& pass, const string& id, const string& diskPath = "");
void cerrarSesion();
bool haySesionActiva();
string getUsuarioActual();
string getIdParticionActual();
string getPathDiscoActual();

#endif
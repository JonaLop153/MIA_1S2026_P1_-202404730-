#include "session.h"

static MountMap* particionesMontadasPtr = nullptr;
static SesionActiva* sesionActivaPtr = nullptr;

MountMap& getParticionesMontadas() {
    if (!particionesMontadasPtr) {
        particionesMontadasPtr = new MountMap();
    }
    return *particionesMontadasPtr;
}

SesionActiva& getSesionActiva() {
    if (!sesionActivaPtr) {
        sesionActivaPtr = new SesionActiva();
        sesionActivaPtr->activa = false;
        sesionActivaPtr->indiceParticion = -1;
    }
    return *sesionActivaPtr;
}

void mountPartition(const string& id, const string& path, int indice) {
    getParticionesMontadas()[id] = make_pair(path, indice);
}

void unmountPartition(const string& id) {
    getParticionesMontadas().erase(id);
}

bool isMounted(const string& path, int indice) {
    for (auto const& [id, data] : getParticionesMontadas()) {
        if (data.first == path && data.second == indice) {
            return true;
        }
    }
    return false;
}

string getMountID(const string& path, int indice) {
    for (auto const& [id, data] : getParticionesMontadas()) {
        if (data.first == path && data.second == indice) {
            return id;
        }
    }
    return "";
}

void iniciarSesion(const string& user, const string& pass, const string& id, const string& diskPath) {
    SesionActiva& sesion = getSesionActiva();
    sesion.usuario = user;
    sesion.password = pass;
    sesion.idParticion = id;
    sesion.pathDisco = diskPath;
    sesion.activa = true;
}

void cerrarSesion() {
    SesionActiva& sesion = getSesionActiva();
    sesion.usuario = "";
    sesion.password = "";
    sesion.idParticion = "";
    sesion.pathDisco = "";
    sesion.indiceParticion = -1;
    sesion.activa = false;
}

bool haySesionActiva() {
    return getSesionActiva().activa;
}

string getUsuarioActual() {
    return getSesionActiva().usuario;
}

string getIdParticionActual() {
    return getSesionActiva().idParticion;
}

string getPathDiscoActual() {
    return getSesionActiva().pathDisco;
}
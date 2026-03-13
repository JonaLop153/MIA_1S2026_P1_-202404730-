#ifndef MOUNT_H
#define MOUNT_H

#include <string>
#include <map>

using namespace std;

class Mount {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static string generarID(const string& path);
    static bool buscarParticion(const string& path, const string& name, char type, int& indice, char& partType);
};

#endif
#ifndef MKFILE_H
#define MKFILE_H

#include <string>
#include <map>
using namespace std;

class MkFile {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static bool crearArchivo(const string& id, const string& path, int size, const string& cont, bool r);
};

#endif
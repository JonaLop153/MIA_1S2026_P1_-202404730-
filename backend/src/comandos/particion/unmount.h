#ifndef UNMOUNT_H
#define UNMOUNT_H

#include <string>
#include <map>

using namespace std;

class Unmount {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
};

#endif
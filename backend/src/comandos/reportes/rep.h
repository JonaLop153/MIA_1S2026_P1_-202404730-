#ifndef REP_H
#define REP_H

#include <string>
#include <map>
#include "../../structs.h"
#include "../../session/session.h"

using namespace std;

class Rep {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static bool obtenerPathDisco(const string& id, string& path, int& indice);
    static string escapeDot(const string& texto);
    static string generarReporteMBR(const string& diskPath);
    static string generarReporteDisk(const string& diskPath);
};

#endif
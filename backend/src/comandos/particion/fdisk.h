#ifndef FDISK_H
#define FDISK_H

#include <string>
#include <map>
#include "../../structs.h"  // ← ¡ESTO ES LO QUE FALTA!

using namespace std;

class FDisk {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static long long calcularTamañoBytes(long long size, char unit);
    static bool leerMBR(const string& path, MBR& mbr);
    static bool escribirMBR(const string& path, const MBR& mbr);
    static int buscarEspacioLibre(const MBR& mbr, long long tamaño, char fit);
    static bool crearParticionPrimaria(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes);
    static bool crearParticionExtendida(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes);
    static bool crearParticionLogica(const string& path, MBR& mbr, const map<string, string>& params, long long tamañoBytes);
};

#endif
#ifndef MKDISK_H
#define MKDISK_H

#include <string>
#include <map>

using namespace std;

class MKDisk {
public:
    // Parsear parámetros del comando MKDISK
    static map<string, string> parsearParametros(const string& comando);
    
    // Ejecutar el comando MKDISK
    static string ejecutar(const string& comando);
    
private:
    // Validar parámetros
    static bool validarParametros(const map<string, string>& params, string& error);
    
    // Calcular tamaño en bytes según unit
    static long long calcularTamañoBytes(long long size, char unit);
    
    // Crear archivo binario con ceros
    static bool crearArchivoBinario(const string& path, long long tamaño);
    
    // Escribir MBR en el archivo
    static bool escribirMBR(const string& path, const map<string, string>& params);
};

#endif
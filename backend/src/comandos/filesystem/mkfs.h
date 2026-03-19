#ifndef MKFS_H
#define MKFS_H

#include <string>
#include <map>
#include "../../structs.h"
#include "../../session/session.h"

using namespace std;

class MKFS {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static bool buscarParticionMontada(const string& id, string& path, int& indice);
    static bool crearUsersTXT(const string& path, int partStart, int inodeTableOffset, int blockTableOffset, int blockSize, int nextFreeBlock);
};

#endif
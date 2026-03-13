#ifndef MKFS_H
#define MKFS_H

#include <string>
#include <map>
#include "../../structs.h"

using namespace std;

class MKFS {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static bool buscarParticionMontada(const string& id, string& path, int& indice);
    static bool escribirSuperBlock(const string& path, int start, SuperBlock& sb);
    static bool escribirBitmaps(const string& path, int start, int totalInodos, int totalBloques);
    static bool escribirInodeTable(const string& path, int start, int totalInodos);
    static bool crearInodoRaiz(const string& path, int inodeStart, int blockStart, int blockSize);
    static bool crearUsersTXT(const string& path, int inodeStart, int blockStart, int blockSize, int nextFreeBlock);
};

#endif
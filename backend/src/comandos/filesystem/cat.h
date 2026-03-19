#ifndef CAT_H
#define CAT_H
#include <string>
#include <map>
using namespace std;
class Cat {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
};
#endif
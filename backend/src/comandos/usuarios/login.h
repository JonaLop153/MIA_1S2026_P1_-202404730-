#ifndef LOGIN_H
#define LOGIN_H

#include <string>
#include <map>

using namespace std;

class Login {
public:
    static map<string, string> parsearParametros(const string& comando);
    static string ejecutar(const string& comando);
private:
    static bool validarParametros(const map<string, string>& params, string& error);
    static bool autenticarUsuario(const string& id, const string& user, const string& pass);
};

#endif
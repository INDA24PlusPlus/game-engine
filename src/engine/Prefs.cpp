#include "prefs.h"

#include <fstream>
#include <iostream>
#include <sstream>

Prefs::Prefs(const char *path) {
    std::ifstream file(path);
    if (!file) {
        std::ofstream new_file(path);
        if (!new_file) {
            std::cout << "ERR: Failed to create prefs file" << '\n';
        }
        new_file.close();
    } else {
        // Load existing prefs from file
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream file(line);
            std::string key, value, type;
            if (std::getline(file, key, ' ') && std::getline(file, value, ' ') && std::getline(file, type)) {
                PrefValueType value_type;
                if (type == "int")
                    value_type = PrefValueType::INT;
                else if (type == "float")
                    value_type = PrefValueType::FLOAT;
                else if (type == "bool")
                    value_type = PrefValueType::BOOL;
                else if (type == "string")
                    value_type = PrefValueType::STRING;
                else {
                    std::cout << "WARN: Unknown type " << type << " for prefs entry " << key << '\n';
                    continue;
                }

                internal_storage[key.c_str()] = PrefEntry{.type = value_type, .value = value};
            }
        }
    }
}

Prefs::~Prefs() { save(); }

bool Prefs::contains(const char *key) { return false; }

bool Prefs::remove(const char *key) { return false; }

bool Prefs::clear() { return false; }

void Prefs::save() {}

void Prefs::set_int(const char *key, int value, bool save = true) {}

void Prefs::set_float(const char *key, float value, bool save = true) {}

void Prefs::set_bool(const char *key, bool value, bool save = true) {}

void Prefs::set_string(const char *key, const std::string &value, bool save = true) {}

int Prefs::get_int(const char *key) { return 0; }

float Prefs::get_float(const char *key) { return 0.0f; }

bool Prefs::get_bool(const char *key) { return false; }

std::string Prefs::get_string(const char *key) { return std::string(); }

int Prefs::get_int_or(const char *key, int fallback) { return 0; }

float Prefs::get_float_or(const char *key, float fallback) { return 0.0f; }

bool Prefs::get_bool_or(const char *key, bool fallback) { return false; }

std::string Prefs::get_string_or(const char *key, const std::string &fallback) { return std::string(); }

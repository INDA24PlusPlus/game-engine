#include "Prefs.h"

#include <fstream>
#include <iostream>
#include <sstream>

// personal todo:
// - test everything
// - implement flush
// - better error handling
// - follow project conventions (logging etc)
// - maybe an array type or vec2/3/4 type

Prefs::Prefs(const char *path) : prefs_path(path) {
    std::ifstream file(path);
    if (!file) {
        // Create new prefs file
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

                // parse type
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

                // save to internal storage
                internal_storage[key.c_str()] = PrefEntry{.type = value_type, .value = value};
            }
        }
    }
}

Prefs::~Prefs() { flush(); }

bool Prefs::contains(const char *key) { return internal_storage.find(key) != internal_storage.end(); }

bool Prefs::remove(const char *key) { internal_storage.erase(key); }

void Prefs::clear() { internal_storage.clear(); }

void Prefs::flush() {
    // TODO
}

void Prefs::put_int(const char *key, int value) {
    internal_storage[key] = PrefEntry{.type = PrefValueType::INT, .value = std::to_string(value)};
}

void Prefs::put_float(const char *key, float value) {
    internal_storage[key] = PrefEntry{.type = PrefValueType::FLOAT, .value = std::to_string(value)};
}

void Prefs::put_bool(const char *key, bool value) {
    internal_storage[key] = PrefEntry{.type = PrefValueType::BOOL, .value = value ? "true" : "false"};
}

void Prefs::put_string(const char *key, const std::string &value) {
    internal_storage[key] = PrefEntry{.type = PrefValueType::STRING, .value = value};
}

int Prefs::get_int(const char *key) { return std::stoi(internal_storage[key].value); }

float Prefs::get_float(const char *key) { return std::stof(internal_storage[key].value); }

bool Prefs::get_bool(const char *key) { return internal_storage[key].value == "true"; }

std::string Prefs::get_string(const char *key) { return internal_storage[key].value; }

int Prefs::get_int_or(const char *key, int fallback) {
    if (!contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::INT)
        return fallback;

    return get_int(key);
}

float Prefs::get_float_or(const char *key, float fallback) {
    if (!contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::FLOAT)
        return fallback;

    return get_float(key);
}

bool Prefs::get_bool_or(const char *key, bool fallback) {
    if (!contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::BOOL)
        return fallback;

    return get_bool(key);
}

std::string Prefs::get_string_or(const char *key, const std::string &fallback) {
    if (!contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::STRING)
        return fallback;

    return get_string(key);
}

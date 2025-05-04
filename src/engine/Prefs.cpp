#include "Prefs.h"

#include <fstream>
#include <iostream>
#include <sstream>

Prefs::Prefs(const std::string &path) : prefs_path(path) {
    std::ifstream file(path);
    if (!file) {
        // Create new prefs file
        std::ofstream new_file(path);
        if (!new_file) {
            ERROR("Failed to create prefs file");
        }
        new_file.close();
    } else {
        // Load existing prefs from file
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream line_stream(line);
            std::string key, value, type;
            if (std::getline(line_stream, key, ' ') && std::getline(line_stream, type, ' ') &&
                std::getline(line_stream, value)) {
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
                else if (type == "vec2")
                    value_type = PrefValueType::VEC2;
                else if (type == "vec3")
                    value_type = PrefValueType::VEC3;
                else if (type == "vec4")
                    value_type = PrefValueType::VEC4;
                else {
                    WARN("Unknown type %s for prefs entry %s", type.c_str(), key.c_str());
                    continue;
                }

                // save to internal storage
                internal_storage[key.c_str()] = PrefEntry{.type = value_type, .value = value};
            }
        }
    }
}

Prefs::~Prefs() { flush(); }

bool Prefs::contains(const std::string &key) { return internal_storage.find(key) != internal_storage.end(); }

void Prefs::remove(const std::string &key) { internal_storage.erase(key); }

void Prefs::clear() { internal_storage.clear(); }

void Prefs::flush() {
    std::ofstream file(this->prefs_path);
    if (!file)
        return;

    for (const auto &pair : internal_storage) {
        // parse type
        std::string type;
        switch (pair.second.type) {
        case PrefValueType::INT:
            type = "int";
            break;
        case PrefValueType::FLOAT:
            type = "float";
            break;
        case PrefValueType::BOOL:
            type = "bool";
            break;
        case PrefValueType::STRING:
            type = "string";
            break;
        case PrefValueType::VEC2:
            type = "vec2";
            break;
        case PrefValueType::VEC3:
            type = "vec3";
            break;
        case PrefValueType::VEC4:
            type = "vec4";
            break;
        }

        file << pair.first << ' ' << type << ' ' << pair.second.value << '\n';
    }

    file.close();
}

inline bool validate_key(const std::string &key) {
    if (key.empty()) {
        ERROR("Key cannot be empty");
        return false;
    }

    if (key.find(' ') != std::string::npos) {
        ERROR("Key cannot contain spaces");
        return false;
    }

    return true;
}

void Prefs::put_int(const std::string &key, int value) {
    if (!validate_key(key))
        return;

    internal_storage[key] = PrefEntry{.type = PrefValueType::INT, .value = std::to_string(value)};
}

void Prefs::put_float(const std::string &key, float value) {
    if (!validate_key(key))
        return;

    internal_storage[key] = PrefEntry{.type = PrefValueType::FLOAT, .value = std::to_string(value)};
}

void Prefs::put_bool(const std::string &key, bool value) {
    if (!validate_key(key))
        return;

    internal_storage[key] = PrefEntry{.type = PrefValueType::BOOL, .value = value ? "true" : "false"};
}

void Prefs::put_string(const std::string &key, const std::string &value) {
    if (!validate_key(key))
        return;

    internal_storage[key] = PrefEntry{.type = PrefValueType::STRING, .value = value};
}

void Prefs::put_vec2(const std::string &key, const glm::vec2 &value) {
    if (!validate_key(key))
        return;

    internal_storage[key] =
        PrefEntry{.type = PrefValueType::VEC2, .value = std::to_string(value.x) + ',' + std::to_string(value.y)};
}

void Prefs::put_vec3(const std::string &key, const glm::vec3 &value) {
    if (!validate_key(key))
        return;

    internal_storage[key] =
        PrefEntry{.type = PrefValueType::VEC3,
                  .value = std::to_string(value.x) + ',' + std::to_string(value.y) + ',' + std::to_string(value.z)};
}

void Prefs::put_vec4(const std::string &key, const glm::vec4 &value) {
    if (!validate_key(key))
        return;

    internal_storage[key] = PrefEntry{.type = PrefValueType::VEC4,
                                      .value = std::to_string(value.x) + ',' + std::to_string(value.y) + ',' +
                                               std::to_string(value.z) + ',' + std::to_string(value.w)};
}

int Prefs::get_int(const std::string &key) {
    if (!validate_key(key))
        return 0;

    return std::stoi(internal_storage[key].value);
}

float Prefs::get_float(const std::string &key) {
    if (!validate_key(key))
        return 0.f;

    return std::stof(internal_storage[key].value);
}

bool Prefs::get_bool(const std::string &key) {
    if (!validate_key(key))
        return false;

    return internal_storage[key].value == "true";
}

std::string Prefs::get_string(const std::string &key) {
    if (!validate_key(key))
        return;

    return internal_storage[key].value;
}

glm::vec2 Prefs::get_vec2(const std::string &key) {
    if (!validate_key(key))
        return glm::vec2(0.f);

    std::istringstream iss(internal_storage[key].value);
    std::string x_str, y_str;
    std::getline(iss, x_str, ',');
    std::getline(iss, y_str, ',');

    return glm::vec2(std::stof(x_str), std::stof(y_str));
}

glm::vec3 Prefs::get_vec3(const std::string &key) {
    if (!validate_key(key))
        return glm::vec3(0.f);

    std::istringstream iss(internal_storage[key].value);
    std::string x_str, y_str, z_str;
    std::getline(iss, x_str, ',');
    std::getline(iss, y_str, ',');
    std::getline(iss, z_str, ',');

    return glm::vec3(std::stof(x_str), std::stof(y_str), std::stof(z_str));
}

glm::vec4 Prefs::get_vec4(const std::string &key) {
    if (!validate_key(key))
        return glm::vec4(0.f);

    std::istringstream iss(internal_storage[key].value);
    std::string x_str, y_str, z_str, w_str;
    std::getline(iss, x_str, ',');
    std::getline(iss, y_str, ',');
    std::getline(iss, z_str, ',');
    std::getline(iss, w_str, ',');

    return glm::vec4(std::stof(x_str), std::stof(y_str), std::stof(z_str), std::stof(w_str));
}

int Prefs::get_int_or(const std::string &key, int fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::INT)
        return fallback;

    return get_int(key);
}

float Prefs::get_float_or(const std::string &key, float fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::FLOAT)
        return fallback;

    return get_float(key);
}

bool Prefs::get_bool_or(const std::string &key, bool fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::BOOL)
        return fallback;

    return get_bool(key);
}

std::string Prefs::get_string_or(const std::string &key, const std::string &fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::STRING)
        return fallback;

    return get_string(key);
}

glm::vec2 Prefs::get_vec2_or(const std::string &key, const glm::vec2 &fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::VEC2)
        return fallback;

    return get_vec2(key);
}

glm::vec3 Prefs::get_vec3_or(const std::string &key, const glm::vec3 &fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::VEC3)
        return fallback;

    return get_vec3(key);
}

glm::vec4 Prefs::get_vec4_or(const std::string &key, const glm::vec4 &fallback) {
    if (!validate_key(key) || !contains(key))
        return fallback;

    if (internal_storage[key].type != PrefValueType::VEC4)
        return fallback;

    return get_vec4(key);
}
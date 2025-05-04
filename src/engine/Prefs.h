#pragma once

#include "logging.h"
#include <glm/glm.hpp>
#include "utils/logging.h"
#include <string>
#include <unordered_map>

enum class PrefValueType { INT, FLOAT, BOOL, STRING, VEC2, VEC3, VEC4 };

struct PrefEntry {
    PrefValueType type;
    std::string value;
};

class Prefs {
  public:
    Prefs() = delete;
    Prefs(const std::string &path);
    ~Prefs();

    bool contains(const std::string &key);
    void remove(const std::string &key);
    void clear();

    // save all prefs to the file
    void flush();

    // write to prefs
    void put_int(const std::string &key, int value);
    void put_float(const std::string &key, float value);
    void put_bool(const std::string &key, bool value);
    void put_string(const std::string &key, const std::string &value);
    void put_vec2(const std::string &key, const glm::vec2 &value);
    void put_vec3(const std::string &key, const glm::vec3 &value);
    void put_vec4(const std::string &key, const glm::vec4 &value);

    // get prefs (unsafe)
    int get_int(const std::string &key);
    float get_float(const std::string &key);
    bool get_bool(const std::string &key);
    std::string get_string(const std::string &key);
    glm::vec2 get_vec2(const std::string &key);
    glm::vec3 get_vec3(const std::string &key);
    glm::vec4 get_vec4(const std::string &key);

    // get prefs with fallback value
    int get_int_or(const std::string &key, int fallback);
    float get_float_or(const std::string &key, float fallback);
    bool get_bool_or(const std::string &key, bool fallback);
    std::string get_string_or(const std::string &key, const std::string &fallback);
    glm::vec2 get_vec2_or(const std::string &key, const glm::vec2 &fallback);
    glm::vec3 get_vec3_or(const std::string &key, const glm::vec3 &fallback);
    glm::vec4 get_vec4_or(const std::string &key, const glm::vec4 &fallback);

  private:
    std::string prefs_path;
    std::unordered_map<std::string, PrefEntry> internal_storage;
};

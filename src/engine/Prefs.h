#pragma once

#include <string>
#include <unordered_map>

enum class PrefValueType { INT, FLOAT, BOOL, STRING };

struct PrefEntry {
	PrefValueType type;
	std::string value;
};

class Prefs {
public:
	Prefs() = delete;
	Prefs(const std::string& path);
	~Prefs();

	bool contains(const std::string& key);
	void remove(const std::string& key);
	void clear();
	// Save all prefs to the file
	void flush();

	void put_int(const std::string &key, int value);
	void put_float(const std::string& key, float value);
	void put_bool(const std::string& key, bool value);
	void put_string(const std::string& key, const std::string& value);

	int get_int(const std::string& key);
	float get_float(const std::string& key);
	bool get_bool(const std::string& key);
	std::string get_string(const std::string& key);

	int get_int_or(const std::string& key, int fallback);
	float get_float_or(const std::string& key, float fallback);
	bool get_bool_or(const std::string& key, bool fallback);
	std::string get_string_or(const std::string& key, const std::string& fallback);

private:
	std::string prefs_path;
	std::unordered_map<std::string, PrefEntry> internal_storage;
};

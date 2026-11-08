#include "crow.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <mutex>
#include <random>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;
namespace {
std::mutex sessionMutex;
std::vector<std::string> sessions;

std::string env(const char* key, const char* fallback = "") {
    const char* value = std::getenv(key);
    return value && *value ? value : fallback;
}
std::string dataDir() { return env("VV_DATA_DIR", "./data"); }
std::string mediaDir() { return dataDir() + "/uploads"; }

std::string randomToken() {
    std::random_device rd;
    static constexpr char hex[] = "0123456789abcdef";
    std::string out(64, '0');
    for (char& c : out) c = hex[rd() % 16];
    return out;
}

void addCors(crow::response& response) {
    const std::string origin = env("VV_ALLOWED_ORIGIN", "https://visa-vistapk.vercel.app");
    response.set_header("Access-Control-Allow-Origin", origin);
    response.set_header("Vary", "Origin");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
}
crow::response jsonResponse(int status, crow::json::wvalue body) {
    crow::response response(std::move(body));
    response.code = status;
    response.set_header("Content-Type", "application/json; charset=utf-8");
    addCors(response);
    return response;
}
crow::response textResponse(int status, const std::string& body) {
    crow::response response(status, body);
    addCors(response);
    return response;
}

sqlite3* openDb() {
    sqlite3* database = nullptr;
    const std::string path = dataDir() + "/visa_vista.db";
    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
        if (database) sqlite3_close(database);
        return nullptr;
    }
    sqlite3_busy_timeout(database, 5000);
    return database;
}
void initialize() {
    fs::create_directories(mediaDir());
    sqlite3* database = openDb();
    if (!database) throw std::runtime_error("Could not open SQLite database. Check VV_DATA_DIR permissions.");
    const char* sql = "CREATE TABLE IF NOT EXISTS stories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL DEFAULT '',"
        "category TEXT NOT NULL DEFAULT '', country TEXT NOT NULL DEFAULT '',"
        "date TEXT NOT NULL DEFAULT '', story TEXT NOT NULL DEFAULT '',"
        "image TEXT NOT NULL DEFAULT '', video TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);";
    char* error = nullptr;
    const int result = sqlite3_exec(database, sql, nullptr, nullptr, &error);
    std::string message = error ? error : "database initialization failed";
    sqlite3_free(error);
    sqlite3_close(database);
    if (result != SQLITE_OK) throw std::runtime_error(message);
}

bool authorized(const crow::request& request) {
    const std::string header = request.get_header_value("Authorization");
    if (header.rfind("Bearer ", 0) != 0) return false;
    const std::string supplied = header.substr(7);
    std::lock_guard<std::mutex> lock(sessionMutex);
    return std::find(sessions.begin(), sessions.end(), supplied) != sessions.end();
}
std::string base64Decode(const std::string& input) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    int value = 0, bits = -8;
    for (unsigned char ch : input) {

#include "crow.h"
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>

std::mutex sessionMutex;
std::vector<std::string> sessions;
std::string DATA_FILE = "stories.json";
std::mutex fileMutex;

std::string load_file() {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ifstream in(DATA_FILE);
    if (!in) return "{\"stories\":[]}";
    std::stringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    if (s.empty() || s.find("stories") == std::string::npos) return "{\"stories\":[]}";
    return s;
}
void save_file(const std::string& content) {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ofstream out(DATA_FILE);
    out << content;
}
std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}
std::string get_str(crow::json::rvalue& body, const char* key) {
    try {
        if (!body.has(key)) return "";
        auto v = body[key];
        if (v.t() == crow::json::type::String) return std::string(v.s());
        return "";
    } catch (...) { return ""; }
}
bool authorized(const crow::request& req) {
    std::string supplied = "";
    std::string header = req.get_header_value("Authorization");
    if (header.size() > 7 && header.rfind("Bearer ", 0) == 0) supplied = header.substr(7);
    if (supplied.empty()) {
        const char* q = req.url_params.get("token");
        if (q) supplied = std::string(q);
    }
    if (supplied.empty()) {
        std::string url = req.url;
        size_t pos = url.find("token=");
        if (pos != std::string::npos) {
            supplied = url.substr(pos + 6);
            size_t amp = supplied.find('&');
            if (amp != std::string::npos) supplied = supplied.substr(0, amp);
        }
    }
    if (supplied.empty()) return false;
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (auto &s : sessions) if (s == supplied) return true;
    return false;
}
std::string make_token() {
    return "vv-" + std::to_string(std::rand()) + "-" + std::to_string(time(nullptr));
}
void add_cors(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}
int main() {
    std::srand((unsigned)time(nullptr));
    crow::SimpleApp app;
    CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
    ([](const crow::request& req, std::string path) {
        crow::response res(204);
        add_cors(res);
        return res;
    });
    CROW_ROUTE(app, "/").methods("GET"_method)
    ([](const crow::request& req) {
        crow::response res("Visa Vista API Running");
        add_cors(res);
        return res;
    });
    CROW_ROUTE(app, "/api/health").methods("GET"_method)
    ([](const crow::request& req) {
        crow::response res("{\"ok\":true}");
        res.set_header("Content-Type", "application/json");
        add_cors(res);
        return res;
    });
    CROW_ROUTE(app, "/api/stories").methods("GET"_method)
    ([](const crow::request& req) {
        crow::response res(load_file());
        res.set_header("Content-Type", "application/json");
        add_cors(res);
        return res;
    });
    CROW_ROUTE(app, "/api/login").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        auto body = crow::json::load(req.body);
        if (!body) { res.code = 400; res.body = "{\"error\":\"bad json\"}"; return res; }
        std::string pass = get_str(body, "password");
        const char* env_pass = std::getenv("ADMIN_PASSWORD");
        std::string correct = env_pass ? env_pass : "admin123";
        if (pass != correct) { res.code = 401; res.body = "{\"error\":\"wrong password\"}"; return res; }
        std::string token = make_token();
        { std::lock_guard<std::mutex> lock(sessionMutex); sessions.push_back(token); }
        res.code = 200;
        res.body = "{\"token\":\"" + token + "\"}";
        return res;
    });
    CROW_ROUTE(app, "/api/logout").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"login again\"}"; return res; }
        std::string supplied = "";
        const char* q = req.url_params.get("token");
        if (q) supplied = std::string(q);
        { std::lock_guard<std::mutex> lock(sessionMutex);
          sessions.erase(std::remove(sessions.begin(), sessions.end(), supplied), sessions.end()); }
        res.code = 200; res.body = "{\"ok\":true}"; return res;
    });
    CROW_ROUTE(app, "/api/stories").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"Please log in again\"}"; return res; }
        auto body = crow::json::load(req.body);
        if (!body) { res.code = 400; res.body = "{\"error\":\"bad json\"}"; return res; }
        std::string id = std::to_string(time(nullptr)) + "-" + std::to_string(std::rand());
        std::string newStory = "{\"id\":\"" + id + "\","
            "\"title\":\"" + json_escape(get_str(body, "title")) + "\","
            "\"category\":\"" + json_escape(get_str(body, "category")) + "\","
            "\"country\":\"" + json_escape(get_str(body, "country")) + "\","
            "\"date\":\"" + json_escape(get_str(body, "date")) + "\","
            "\"story\":\"" + json_escape(get_str(body, "story")) + "\","
            "\"image\":\"" + json_escape(get_str(body, "image")) + "\","
            "\"video\":\"" + json_escape(get_str(body, "video")) + "\"}";
        std::string old = load_file();
        std::string out;
        if (old == "{\"stories\":[]}") {
            out = "{\"stories\":[" + newStory + "]}";
        } else {
            size_t pos = old.rfind("]}");
            if (pos == std::string::npos) out = "{\"stories\":[" + newStory + "]}";
            else {
                std::string inner = old.substr(0, pos);
                if (inner.find("\"id\"") != std::string::npos) inner += ",";
                inner += newStory + "]}";
                out = inner;
            }
        }
        save_file(out);
        res.code = 200; res.body = "{\"ok\":true}"; return res;
    });
    CROW_ROUTE(app, "/api/stories/<string>").methods("DELETE"_method)
    ([](const crow::request& req, std::string id) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"Please log in again\"}"; return res; }
        res.code = 200; res.body = "{\"ok\":true}"; return res;
    });
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::atoi(port_env) : 18080;
    app.port(port).multithreaded().run();
    return 0;
}

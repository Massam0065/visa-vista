#include "crow.h"
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>

std::mutex sessionMutex;
std::vector<std::string> sessions;
std::mutex fileMutex;

std::string get_data_file() {
    const char* dir = std::getenv("VV_DATA_DIR");
    if (dir && std::string(dir).size() > 0) {
        std::string d = std::string(dir);
        try { std::filesystem::create_directories(d); } catch(...) {}
        if (d.back() != '/' && d.back() != '\\') d += "/";
        return d + "stories.json";
    }
    return "stories.json";
}

std::string load_file() {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ifstream in(get_data_file());
    if (!in) return "{\"stories\":[]}";
    std::stringstream ss; ss << in.rdbuf();
    std::string s = ss.str();
    if (s.empty() || s.find("stories") == std::string::npos) return "{\"stories\":[]}";
    return s;
}
void save_file(const std::string& content) {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::string path = get_data_file();
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    out << content;
    out.flush();
}
std::string json_escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
        else out += (char)c;
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
    if (supplied.empty()) return false;
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (auto &s : sessions) if (s == supplied) return true;
    return false;
}
std::string make_token() { return "vv-" + std::to_string(std::rand()) + "-" + std::to_string(time(nullptr)); }

void add_cors(crow::response& res, const crow::request& req) {
    std::string origin = req.get_header_value("Origin");
    if (origin.empty()) origin = "*";
    res.set_header("Access-Control-Allow-Origin", origin);
    res.set_header("Vary", "Origin");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Max-Age", "86400");
}
void add_cors(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Vary", "Origin");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Max-Age", "86400");
}

int main() {
    std::srand((unsigned)time(nullptr));
    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/login").methods("OPTIONS"_method)
    ([](const crow::request& req) { crow::response res(204); add_cors(res, req); return res; });
    CROW_ROUTE(app, "/api/stories").methods("OPTIONS"_method)
    ([](const crow::request& req) { crow::response res(204); add_cors(res, req); return res; });
    CROW_ROUTE(app, "/api/stories/<string>").methods("OPTIONS"_method)
    ([](const crow::request& req, std::string id) { crow::response res(204); add_cors(res, req); return res; });
    CROW_ROUTE(app, "/api/delete").methods("OPTIONS"_method)
    ([](const crow::request& req) { crow::response res(204); add_cors(res, req); return res; });
    CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
    ([](const crow::request& req, std::string path) { crow::response res(204); add_cors(res, req); return res; });

    CROW_ROUTE(app, "/").methods("GET"_method)
    ([](const crow::request& req) { crow::response res("Visa Vista API Running"); add_cors(res, req); return res; });

    CROW_ROUTE(app, "/api/health").methods("GET"_method)
    ([](const crow::request& req) { crow::response res("{\"ok\":true}"); res.set_header("Content-Type","application/json"); add_cors(res, req); return res; });

    CROW_ROUTE(app, "/api/stories").methods("GET"_method)
    ([](const crow::request& req) { crow::response res(load_file()); res.set_header("Content-Type","application/json"); add_cors(res, req); return res; });

    CROW_ROUTE(app, "/api/login").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res; add_cors(res, req); res.set_header("Content-Type","application/json");
        auto body = crow::json::load(req.body);
        if (!body) { res.code=400; res.body="{\"error\":\"bad json\"}"; return res; }
        std::string pass = get_str(body, "password");
        const char* env_pass = std::getenv("VV_ADMIN_PASSWORD");
        if (!env_pass) env_pass = std::getenv("ADMIN_PASSWORD");
        std::string correct = (env_pass && std::string(env_pass).size() > 0) ? std::string(env_pass) : "admin123";
        if (pass != correct) { res.code=401; res.body="{\"error\":\"wrong password\"}"; return res; }
        std::string token = make_token();
        { std::lock_guard<std::mutex> lock(sessionMutex); sessions.push_back(token); }
        res.code=200; res.body="{\"token\":\""+token+"\"}"; return res;
    });

    CROW_ROUTE(app, "/api/stories").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res; add_cors(res, req); res.set_header("Content-Type","application/json");
        if (!authorized(req)) { res.code=401; res.body="{\"error\":\"Please log in again\"}"; return res; }
        auto body = crow::json::load(req.body);
        if (!body) { res.code=400; res.body="{\"error\":\"bad json\"}"; return res; }
        std::string id = std::to_string(time(nullptr)) + "-" + std::to_string(std::rand());
        std::string ns = "{\"id\":\""+id+"\",\"title\":\""+json_escape(get_str(body,"title"))+"\",\"category\":\""+json_escape(get_str(body,"category"))+"\",\"country\":\""+json_escape(get_str(body,"country"))+"\",\"date\":\""+json_escape(get_str(body,"date"))+"\",\"story\":\""+json_escape(get_str(body,"story"))+"\",\"image\":\""+json_escape(get_str(body,"image"))+"\",\"video\":\""+json_escape(get_str(body,"video"))+"\"}";
        std::string old = load_file(); std::string out;
        if (old=="{\"stories\":[]}") out="{\"stories\":["+ns+"]}";
        else {
            size_t pos=old.rfind("]}");
            if(pos==std::string::npos) out="{\"stories\":["+ns+"]}";
            else { std::string inner=old.substr(0,pos); if(inner.find("\"id\"")!=std::string::npos) inner+=","; inner+=ns+"]}"; out=inner; }
        }
        save_file(out); res.code=200; res.body="{\"ok\":true,\"id\":\""+id+"\"}"; return res;
    });

    CROW_ROUTE(app, "/api/stories/<string>").methods("DELETE"_method)
    ([](const crow::request& req, std::string id) {
        crow::response res; add_cors(res, req); res.set_header("Content-Type","application/json");
        if (!authorized(req)) { res.code=401; res.body="{\"error\":\"Please log in again\"}"; return res; }
        std::string target = id;
        const char* qid = req.url_params.get("id");
        if (qid && std::string(qid).size()>0) target = std::string(qid);
        std::string old = load_file();
        auto j = crow::json::load(old);
        if (!j || !j.has("stories")) { res.code=200; res.body="{\"ok\":true}"; return res; }
        std::string out = "{\"stories\":[";
        bool first = true;
        bool found = false;
        for (auto &st : j["stories"]) {
            std::string sid = "";
            try { if (st.has("id") && st["id"].t()==crow::json::type::String) sid = std::string(st["id"].s()); } catch(...) {}
            if (sid == target) { found = true; continue; }
            if (!first) out += ",";
            first = false;
            out += "{\"id\":\""+json_escape(sid)+"\"";
            out += ",\"title\":\""+json_escape(get_str(st,"title"))+"\"";
            out += ",\"category\":\""+json_escape(get_str(st,"category"))+"\"";
            out += ",\"country\":\""+json_escape(get_str(st,"country"))+"\"";
            out += ",\"date\":\""+json_escape(get_str(st,"date"))+"\"";
            out += ",\"story\":\""+json_escape(get_str(st,"story"))+"\"";
            out += ",\"image\":\""+json_escape(get_str(st,"image"))+"\"";
            out += ",\"video\":\""+json_escape(get_str(st,"video"))+"\"}";
        }
        out += "]}";
        save_file(out);
        if (found) { res.code=200; res.body="{\"ok\":true,\"deleted\":true}"; }
        else { res.code=200; res.body="{\"ok\":true,\"deleted\":false,\"msg\":\"id not found\"}"; }
        return res;
    });

    // ===== NAYA ADD KIYA HUA - DELETE VIA POST =====
    CROW_ROUTE(app, "/api/delete").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res; add_cors(res, req); res.set_header("Content-Type","application/json");
        auto body = crow::json::load(req.body);
        if (!body) { res.code=400; res.body="{\"error\":\"bad json\"}"; return res; }
        std::string supplied = get_str(body, "token");
        if (supplied.empty()) {
            std::string header = req.get_header_value("Authorization");
            if (header.size() > 7 && header.rfind("Bearer ", 0) == 0) supplied = header.substr(7);
        }
        bool ok = false;
        { std::lock_guard<std::mutex> lock(sessionMutex); for (auto &s : sessions) if (s == supplied) ok = true; }
        if (!ok) { res.code=401; res.body="{\"error\":\"Please log in again\"}"; return res; }
        std::string target = get_str(body, "id");
        std::string old = load_file();
        auto j = crow::json::load(old);
        if (!j || !j.has("stories")) { res.code=200; res.body="{\"ok\":true}"; return res; }
        std::string out = "{\"stories\":[";
        bool first = true;
        bool found = false;
        for (auto &st : j["stories"]) {
            std::string sid = "";
            try { if (st.has("id") && st["id"].t()==crow::json::type::String) sid = std::string(st["id"].s()); } catch(...) {}
            if (sid == target) { found = true; continue; }
            if (!first) out += ",";
            first = false;
            out += "{\"id\":\""+json_escape(sid)+"\"";
            out += ",\"title\":\""+json_escape(get_str(st,"title"))+"\"";
            out += ",\"category\":\""+json_escape(get_str(st,"category"))+"\"";
            out += ",\"country\":\""+json_escape(get_str(st,"country"))+"\"";
            out += ",\"date\":\""+json_escape(get_str(st,"date"))+"\"";
            out += ",\"story\":\""+json_escape(get_str(st,"story"))+"\"";
            out += ",\"image\":\""+json_escape(get_str(st,"image"))+"\"";
            out += ",\"video\":\""+json_escape(get_str(st,"video"))+"\"}";
        }
        out += "]}";
        save_file(out);
        if (found) { res.code=200; res.body="{\"ok\":true,\"deleted\":true}"; }
        else { res.code=200; res.body="{\"ok\":true,\"deleted\":false,\"msg\":\"id not found\"}"; }
        return res;
    });

    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::atoi(port_env) : 18080;
    app.port(port).multithreaded().run(); return 0;
}

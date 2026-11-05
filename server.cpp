#include "crow.h"
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

std::mutex sessionMutex;
std::vector<std::string> sessions;

std::string DATA_FILE = "stories.json";

// ---------- Helper: Load stories file ----------
std::string load_file() {
    std::ifstream in(DATA_FILE);
    if (!in) return "{\"stories\":[]}";
    std::stringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    if (s.empty()) return "{\"stories\":[]}";
    return s;
}

void save_file(const std::string& content) {
    std::ofstream out(DATA_FILE);
    out << content;
}

// ---------- FIXED authorized - Bracket ka masla khatam ----------
bool authorized(const crow::request& request) {
    std::string supplied = "";
    std::string header = request.get_header_value("Authorization");
    if (header.size() > 7 && header.rfind("Bearer ", 0) == 0) {
        supplied = header.substr(7);
    }
    // 1. ?token= se - url_params se (Crow ka sahi tareeqa)
    if (supplied.empty()) {
        const char* q = request.url_params.get("token");
        if (q) supplied = std::string(q);
    }
    // 2. Agar url_params me na mile to URL string se
    if (supplied.empty()) {
        std::string url = request.url;
        size_t pos = url.find("token=");
        if (pos != std::string::npos) {
            supplied = url.substr(pos + 6);
            size_t amp = supplied.find('&');
            if (amp != std::string::npos) supplied = supplied.substr(0, amp);
        }
    }
    if (supplied.empty()) return false;
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (auto &s : sessions) {
        if (s == supplied) return true;
    }
    return false;
}

std::string make_token() {
    static int counter = 0;
    counter++;
    return "vv-" + std::to_string(std::rand()) + "-" + std::to_string(counter) + "-" + std::to_string(time(nullptr));
}

void add_cors(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

int main() {
    crow::SimpleApp app;

    // ---------- CORS Preflight ----------
    CROW_ROUTE(app, "/api/<path>")
    .methods("OPTIONS"_method)
    ([](const crow::request& req, std::string path) {
        crow::response res(204);
        add_cors(res);
        return res;
    });

    // ---------- GET Stories - Public ----------
    CROW_ROUTE(app, "/api/stories")
    .methods("GET"_method)
    ([](const crow::request& req) {
        crow::response res(load_file());
        res.set_header("Content-Type", "application/json");
        add_cors(res);
        return res;
    });

    // ---------- LOGIN ----------
    CROW_ROUTE(app, "/api/login")
    .methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        try {
            auto body = crow::json::load(req.body);
            if (!body) { res.code = 400; res.body = "{\"error\":\"bad json\"}"; return res; }
            std::string pass = body["password"].s();
            const char* env_pass = std::getenv("ADMIN_PASSWORD");
            std::string correct = env_pass ? env_pass : "admin123";
            if (pass != correct) { res.code = 401; res.body = "{\"error\":\"wrong password\"}"; return res; }
            std::string token = make_token();
            {
                std::lock_guard<std::mutex> lock(sessionMutex);
                sessions.push_back(token);
            }
            crow::json::wvalue out;
            out["token"] = token;
            res.code = 200;
            res.body = out.dump();
            return res;
        } catch (...) {
            res.code = 500; res.body = "{\"error\":\"server error\"}"; return res;
        }
    });

    // ---------- LOGOUT ----------
    CROW_ROUTE(app, "/api/logout")
    .methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"login again\"}"; return res; }
        std::string supplied = "";
        const char* q = req.url_params.get("token");
        if (q) supplied = std::string(q);
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            sessions.erase(std::remove(sessions.begin(), sessions.end(), supplied), sessions.end());
        }
        res.code = 200; res.body = "{\"ok\":true}"; return res;
    });

    // ---------- POST Story ----------
    CROW_ROUTE(app, "/api/stories")
    .methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"Please log in again\"}"; return res; }
        try {
            auto body = crow::json::load(req.body);
            if (!body) { res.code = 400; res.body = "{\"error\":\"bad json\"}"; return res; }
            auto data = crow::json::load(load_file());
            crow::json::wvalue stories_val;
            // Purani stories copy karo
            std::vector<crow::json::rvalue> old_list;
            if (data.has("stories")) {
                for (auto& s : data["stories"]) {
                    old_list.push_back(std::move(s));
                }
            }
            // Nayi story banao
            crow::json::wvalue nw;
            nw["id"] = std::to_string(time(nullptr)) + "-" + std::to_string(std::rand());
            nw["title"] = body.has("title") ? std::string(body["title"].s()) : "";
            nw["category"] = body.has("category") ? std::string(body["category"].s()) : "";
            nw["country"] = body.has("country") ? std::string(body["country"].s()) : "";
            nw["date"] = body.has("date") ? std::string(body["date"].s()) : "";
            nw["story"] = body.has("story") ? std::string(body["story"].s()) : "";
            nw["image"] = body.has("image") ? std::string(body["image"].s()) : "";
            nw["video"] = body.has("video") ? std::string(body["video"].s()) : "";

            crow::json::wvalue out;
            crow::json::wvalue::list new_list;
            new_list.push_back(std::move(nw));
            // Purani stories ko wvalue me convert karo
            auto old_json = crow::json::load(load_file());
            if (old_json.has("stories")) {
                for (size_t i = 0; i < old_json["stories"].size(); i++) {
                    crow::json::wvalue item;
                    item["id"] = std::string(old_json["stories"][i]["id"].s());
                    item["title"] = std::string(old_json["stories"][i]["title"].s());
                    item["category"] = old_json["stories"][i].has("category") ? std::string(old_json["stories"][i]["category"].s()) : "";
                    item["country"] = old_json["stories"][i].has("country") ? std::string(old_json["stories"][i]["country"].s()) : "";
                    item["date"] = old_json["stories"][i].has("date") ? std::string(old_json["stories"][i]["date"].s()) : "";
                    item["story"] = old_json["stories"][i].has("story") ? std::string(old_json["stories"][i]["story"].s()) : "";
                    item["image"] = old_json["stories"][i].has("image") ? std::string(old_json["stories"][i]["image"].s()) : "";
                    item["video"] = old_json["stories"][i].has("video") ? std::string(old_json["stories"][i]["video"].s()) : "";
                    new_list.push_back(std::move(item));
                }
            }
            out["stories"] = std::move(new_list);
            save_file(out.dump());
            res.code = 200; res.body = "{\"ok\":true}"; return res;
        } catch (std::exception& e) {
            res.code = 500; res.body = "{\"error\":\"server error\"}"; return res;
        }
    });

    // ---------- DELETE Story ----------
    CROW_ROUTE(app, "/api/stories/<string>")
    .methods("DELETE"_method)
    ([](const crow::request& req, std::string id) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");
        if (!authorized(req)) { res.code = 401; res.body = "{\"error\":\"Please log in again\"}"; return res; }
        try {
            auto old_json = crow::json::load(load_file());
            crow::json::wvalue out;
            crow::json::wvalue::list new_list;
            if (old_json.has("stories")) {
                for (size_t i = 0; i < old_json["stories"].size(); i++) {
                    std::string cur_id = old_json["stories"][i].has("id") ? std::string(old_json["stories"][i]["id"].s()) : "";
                    if (cur_id == id) continue;
                    crow::json::wvalue item;
                    item["id"] = cur_id;
                    item["title"] = std::string(old_json["stories"][i]["title"].s());
                    item["category"] = old_json["stories"][i].has("category") ? std::string(old_json["stories"][i]["category"].s()) : "";
                    item["country"] = old_json["stories"][i].has("country") ? std::string(old_json["stories"][i]["country"].s()) : "";
                    item["date"] = old_json["stories"][i].has("date") ? std::string(old_json["stories"][i]["date"].s()) : "";
                    item["story"] = old_json["stories"][i].has("story") ? std::string(old_json["stories"][i]["story"].s()) : "";
                    item["image"] = old_json["stories"][i].has("image") ? std::string(old_json["stories"][i]["image"].s()) : "";
                    item["video"] = old_json["stories"][i].has("video") ? std::string(old_json["stories"][i]["video"].s()) : "";
                    new_list.push_back(std::move(item));
                }
            }
            out["stories"] = std::move(new_list);
            save_file(out.dump());
            res.code = 200; res.body = "{\"ok\":true}"; return res;
        } catch (...) {
            res.code = 500; res.body = "{\"error\":\"server error\"}"; return res;
        }
    });

    // ---------- Health ----------
    CROW_ROUTE(app, "/")
    ([](const crow::request& req) {
        crow::response res("Visa Vista API Running");
        add_cors(res);
        return res;
    });

    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::atoi(port_env) : 18080;
    app.port(port).multithreaded().run();
    return 0;
}

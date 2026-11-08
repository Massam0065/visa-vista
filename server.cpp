```cpp
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

std::string get_data_file() {
    const char* dir = std::getenv("VV_DATA_DIR");
    if (dir && std::string(dir).size() > 0) {
        std::string d = std::string(dir);
        if (d.back() != '/' && d.back() != '\\') d += "/";
        return d + "stories.json";
    }
    return "stories.json";
}

std::string load_file() {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ifstream in(get_data_file());
    if (!in) return "{\"stories\":[]}";
    std::stringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    if (s.empty() || s.find("stories") == std::string::npos)
        return "{\"stories\":[]}";
    return s;
}

void save_file(const std::string& content) {
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ofstream out(get_data_file());
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
        if (v.t() == crow::json::type::String)
            return std::string(v.s());
        return "";
    } catch (...) {
        return "";
    }
}

bool authorized(const crow::request& req) {
    std::string supplied = "";
    std::string header = req.get_header_value("Authorization");

    if (header.size() > 7 && header.rfind("Bearer ", 0) == 0)
        supplied = header.substr(7);

    if (supplied.empty()) {
        const char* q = req.url_params.get("token");
        if (q) supplied = std::string(q);
    }

    if (supplied.empty()) return false;

    std::lock_guard<std::mutex> lock(sessionMutex);

    for (auto& s : sessions) {
        if (s == supplied) return true;
    }

    return false;
}

std::string make_token() {
    return "vv-" + std::to_string(std::rand()) + "-" +
           std::to_string(time(nullptr));
}

void add_cors(crow::response& res) {
    const char* origin = std::getenv("VV_ALLOWED_ORIGIN");

    if (origin && std::string(origin).size() > 0)
        res.set_header("Access-Control-Allow-Origin", origin);
    else
        res.set_header("Access-Control-Allow-Origin", "*");

    res.set_header("Vary", "Origin");
    res.set_header(
        "Access-Control-Allow-Methods",
        "GET, POST, PUT, DELETE, OPTIONS"
    );
    res.set_header(
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization"
    );
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

        if (!body) {
            res.code = 400;
            res.body = "{\"error\":\"bad json\"}";
            return res;
        }

        std::string pass = get_str(body, "password");

        const char* env_pass = std::getenv("VV_ADMIN_PASSWORD");
        if (!env_pass) env_pass = std::getenv("ADMIN_PASSWORD");

        std::string correct =
            (env_pass && std::string(env_pass).size() > 0)
            ? std::string(env_pass)
            : "admin123";

        if (pass != correct) {
            res.code = 401;
            res.body = "{\"error\":\"wrong password\"}";
            return res;
        }

        std::string token = make_token();

        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            sessions.push_back(token);
        }

        res.code = 200;
        res.body = "{\"token\":\"" + token + "\"}";
        return res;
    });

    // PUBLISH NEW STORY
    CROW_ROUTE(app, "/api/stories").methods("POST"_method)
    ([](const crow::request& req) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");

        if (!authorized(req)) {
            res.code = 401;
            res.body = "{\"error\":\"Please log in again\"}";
            return res;
        }

        auto body = crow::json::load(req.body);

        if (!body) {
            res.code = 400;
            res.body = "{\"error\":\"bad json\"}";
            return res;
        }

        std::string id =
            std::to_string(time(nullptr)) + "-" +
            std::to_string(std::rand());

        std::string ns =
            "{\"id\":\"" + id +
            "\",\"title\":\"" + json_escape(get_str(body, "title")) +
            "\",\"category\":\"" + json_escape(get_str(body, "category")) +
            "\",\"country\":\"" + json_escape(get_str(body, "country")) +
            "\",\"date\":\"" + json_escape(get_str(body, "date")) +
            "\",\"story\":\"" + json_escape(get_str(body, "story")) +
            "\",\"image\":\"" + json_escape(get_str(body, "image")) +
            "\",\"video\":\"" + json_escape(get_str(body, "video")) +
            "\"}";

        std::string old = load_file();
        std::string out;

        if (old == "{\"stories\":[]}") {
            out = "{\"stories\":[" + ns + "]}";
        } else {
            size_t pos = old.rfind("]}");

            if (pos == std::string::npos) {
                out = "{\"stories\":[" + ns + "]}";
            } else {
                std::string inner = old.substr(0, pos);

                if (inner.find("\"id\"") != std::string::npos)
                    inner += ",";

                inner += ns + "]}";
                out = inner;
            }
        }

        save_file(out);

        res.code = 200;
        res.body = "{\"ok\":true}";
        return res;
    });

    // EDIT EXISTING STORY
    CROW_ROUTE(app, "/api/stories/<string>").methods("PUT"_method)
    ([](const crow::request& req, std::string id) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");

        if (!authorized(req)) {
            res.code = 401;
            res.body = "{\"error\":\"Please log in again\"}";
            return res;
        }

        auto body = crow::json::load(req.body);

        if (!body) {
            res.code = 400;
            res.body = "{\"error\":\"bad json\"}";
            return res;
        }

        std::string old = load_file();
        auto data = crow::json::load(old);

        if (!data || !data.has("stories")) {
            res.code = 500;
            res.body = "{\"error\":\"Could not read stories\"}";
            return res;
        }

        crow::json::wvalue::list updatedStories;
        bool found = false;

        for (auto& item : data["stories"]) {
            std::string currentId = get_str(item, "id");

            crow::json::wvalue story;

            if (currentId == id) {
                found = true;

                std::string image = get_str(body, "image");
                std::string video = get_str(body, "video");

                if (image.empty())
                    image = get_str(item, "image");

                if (video.empty())
                    video = get_str(item, "video");

                story["id"] = currentId;
                story["title"] = get_str(body, "title");
                story["category"] = get_str(body, "category");
                story["country"] = get_str(body, "country");
                story["date"] = get_str(body, "date");
                story["story"] = get_str(body, "story");
                story["image"] = image;
                story["video"] = video;
            } else {
                story["id"] = currentId;
                story["title"] = get_str(item, "title");
                story["category"] = get_str(item, "category");
                story["country"] = get_str(item, "country");
                story["date"] = get_str(item, "date");
                story["story"] = get_str(item, "story");
                story["image"] = get_str(item, "image");
                story["video"] = get_str(item, "video");
            }

            updatedStories.push_back(std::move(story));
        }

        if (!found) {
            res.code = 404;
            res.body = "{\"error\":\"Story not found\"}";
            return res;
        }

        crow::json::wvalue output;
        output["stories"] = std::move(updatedStories);

        save_file(output.dump());

        res.code = 200;
        res.body = "{\"ok\":true,\"message\":\"Story updated\"}";
        return res;
    });

    // DELETE STORY
    CROW_ROUTE(app, "/api/stories/<string>").methods("DELETE"_method)
    ([](const crow::request& req, std::string id) {
        crow::response res;
        add_cors(res);
        res.set_header("Content-Type", "application/json");

        if (!authorized(req)) {
            res.code = 401;
            res.body = "{\"error\":\"Please log in again\"}";
            return res;
        }

        std::string old = load_file();
        auto data = crow::json::load(old);

        if (!data || !data.has("stories")) {
            res.code = 500;
            res.body = "{\"error\":\"Could not read stories\"}";
            return res;
        }

        crow::json::wvalue::list remainingStories;
        bool found = false;

        for (auto& item : data["stories"]) {
            std::string currentId = get_str(item, "id");

            if (currentId == id) {
                found = true;
                continue;
            }

            crow::json::wvalue story;
            story["id"] = currentId;
            story["title"] = get_str(item, "title");
            story["category"] = get_str(item, "category");
            story["country"] = get_str(item, "country");
            story["date"] = get_str(item, "date");
            story["story"] = get_str(item, "story");
            story["image"] = get_str(item, "image");
            story["video"] = get_str(item, "video");

            remainingStories.push_back(std::move(story));
        }

        if (!found) {
            res.code = 404;
            res.body = "{\"error\":\"Story not found\"}";
            return res;
        }

        crow::json::wvalue output;
        output["stories"] = std::move(remainingStories);

        save_file(output.dump());

        res.code = 200;
        res.body = "{\"ok\":true}";
        return res;
    });

    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::atoi(port_env) : 18080;

    app.port(port).multithreaded().run();

    return 0;
}
```

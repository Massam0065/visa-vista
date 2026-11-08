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

std::string dataDir() {
    return env("VV_DATA_DIR", "./data");
}

std::string mediaDir() {
    return dataDir() + "/uploads";
}

std::string randomToken() {
    std::random_device rd;
    static constexpr char hex[] = "0123456789abcdef";

    std::string out(64, '0');

    for (char& c : out)
        c = hex[rd() % 16];

    return out;
}

void addCors(crow::response& response) {
    const std::string origin =
        env("VV_ALLOWED_ORIGIN",
            "https://visa-vistapk.vercel.app");

    response.set_header("Access-Control-Allow-Origin", origin);
    response.set_header("Vary", "Origin");
    response.set_header("Access-Control-Allow-Headers",
                        "Content-Type, Authorization");
    response.set_header("Access-Control-Allow-Methods",
                        "GET, POST, DELETE, OPTIONS");
}

crow::response jsonResponse(int status, crow::json::wvalue body) {
    crow::response response(std::move(body));

    response.code = status;

    response.set_header(
        "Content-Type",
        "application/json; charset=utf-8"
    );

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

    const std::string path =
        dataDir() + "/visa_vista.db";

    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
        if (database)
            sqlite3_close(database);

        return nullptr;
    }

    sqlite3_busy_timeout(database, 5000);

    return database;
}

void initialize() {
    fs::create_directories(mediaDir());

    sqlite3* database = openDb();

    if (!database)
        throw std::runtime_error(
            "Could not open SQLite database."
        );

    const char* sql =
        "CREATE TABLE IF NOT EXISTS stories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "title TEXT NOT NULL DEFAULT '',"
        "category TEXT NOT NULL DEFAULT '',"
        "country TEXT NOT NULL DEFAULT '',"
        "date TEXT NOT NULL DEFAULT '',"
        "story TEXT NOT NULL DEFAULT '',"
        "image TEXT NOT NULL DEFAULT '',"
        "video TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* error = nullptr;

    const int result = sqlite3_exec(
        database,
        sql,
        nullptr,
        nullptr,
        &error
    );

    std::string message =
        error ? error : "Database initialization failed";

    sqlite3_free(error);
    sqlite3_close(database);

    if (result != SQLITE_OK)
        throw std::runtime_error(message);
}

bool authorized(const crow::request& request) {
    const std::string header =
        request.get_header_value("Authorization");

    if (header.rfind("Bearer ", 0) != 0)
        return false;

    const std::string supplied = header.substr(7);

    std::lock_guard<std::mutex> lock(sessionMutex);

    return std::find(
        sessions.begin(),
        sessions.end(),
        supplied
    ) != sessions.end();
}

std::string base64Decode(const std::string& input) {
    static const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;

    int value = 0;
    int bits = -8;

    for (unsigned char ch : input) {
        if (ch == '=')
            break;

        const auto pos = alphabet.find(ch);

        if (pos == std::string::npos)
            continue;

        value = (value << 6) + static_cast<int>(pos);
        bits += 6;

        if (bits >= 0) {
            output.push_back(
                static_cast<char>((value >> bits) & 0xff)
            );

            bits -= 8;
        }
    }

    return output;
}

std::string saveDataUrl(
    const std::string& dataUrl,
    bool isVideo
) {
    const auto comma = dataUrl.find(',');

    if (comma == std::string::npos ||
        dataUrl.substr(0, comma).find(";base64") == std::string::npos)
        return "";

    const std::string header =
        dataUrl.substr(0, comma);

    std::string extension;

    if (isVideo)
        extension = header.find("webm") != std::string::npos
            ? "webm" : "mp4";

    else if (header.find("png") != std::string::npos)
        extension = "png";

    else if (header.find("webp") != std::string::npos)
        extension = "webp";

    else if (header.find("jpeg") != std::string::npos ||
             header.find("jpg") != std::string::npos)
        extension = "jpg";

    else
        return "";

    const std::string bytes =
        base64Decode(dataUrl.substr(comma + 1));

    const size_t limit =
        isVideo
            ? 10u * 1024u * 1024u
            : 8u * 1024u * 1024u;

    if (bytes.empty() || bytes.size() > limit)
        return "";

    const std::string filename =
        randomToken() + "." + extension;

    std::ofstream file(
        mediaDir() + "/" + filename,
        std::ios::binary
    );

    if (!file)
        return "";

    file.write(
        bytes.data(),
        static_cast<std::streamsize>(bytes.size())
    );

    if (!file.good())
        return "";

    return "/uploads/" + filename;
}

crow::json::wvalue storyFromRow(sqlite3_stmt* statement) {
    crow::json::wvalue row;

    row["id"] = sqlite3_column_int(statement, 0);

    const char* keys[] = {
        "title",
        "category",
        "country",
        "date",
        "story",
        "image",
        "video",
        "created_at"
    };

    for (int i = 0; i < 8; ++i) {
        const auto* value =
            sqlite3_column_text(statement, i + 1);

        row[keys[i]] =
            value
                ? reinterpret_cast<const char*>(value)
                : "";
    }

    return row;
}

std::string stringField(
    const crow::json::rvalue& json,
    const char* key
) {
    if (!json.has(key) ||
        json[key].t() != crow::json::type::String)
        return "";

    return json[key].s();
}

} // namespace

int main() {

    try {
        initialize();
    }
    catch (const std::exception& error) {
        std::cerr << "Startup error: "
                  << error.what()
                  << std::endl;

        return 1;
    }

    crow::SimpleApp app;

    app.max_payload(16 * 1024 * 1024);

    // OPTIONS ROUTE

    CROW_ROUTE(app, "/api/<path>")
    .methods("OPTIONS"_method)(
        [](const crow::request&, std::string) {

            crow::response response(204);

            addCors(response);

            return response;
        }
    );

    // HEALTH ROUTE

    CROW_ROUTE(app, "/api/health")
    .methods("GET"_method)([] {

        crow::json::wvalue body;

        body["ok"] = true;
        body["service"] = "visa-vista-api";

        return jsonResponse(200, body);
    });

    // LOGIN ROUTE

    CROW_ROUTE(app, "/api/login")
    .methods("POST"_method)(
        [](const crow::request& request) {

            const auto json =
                crow::json::load(request.body);

            if (!json || !json.has("password"))
                return textResponse(
                    400,
                    "Invalid login request"
                );

            const std::string expected =
                env("VV_ADMIN_PASSWORD");

            if (expected.empty())
                return textResponse(
                    503,
                    "Admin password is not configured on the server"
                );

            if (stringField(json, "password") != expected)
                return textResponse(
                    401,
                    "Incorrect password"
                );

            const std::string session = randomToken();

            {
                std::lock_guard<std::mutex> lock(sessionMutex);
                sessions.push_back(session);
            }

            crow::json::wvalue body;

            body["token"] = session;

            return jsonResponse(200, body);
        }
    );

    // GET STORIES ROUTE

    CROW_ROUTE(app, "/api/stories")
    .methods("GET"_method)([] {

        sqlite3* database = openDb();

        if (!database)
            return textResponse(
                500,
                "Database unavailable"
            );

        sqlite3_stmt* statement = nullptr;

        const char* sql =
            "SELECT id,title,category,country,date,"
            "story,image,video,created_at "
            "FROM stories ORDER BY id DESC";

        if (sqlite3_prepare_v2(
            database,
            sql,
            -1,
            &statement,
            nullptr
        ) != SQLITE_OK) {

            sqlite3_close(database);

            return textResponse(
                500,
                "Could not read stories"
            );
        }

        crow::json::wvalue::list stories;

        while (sqlite3_step(statement) == SQLITE_ROW)
            stories.push_back(storyFromRow(statement));

        sqlite3_finalize(statement);
        sqlite3_close(database);

        crow::json::wvalue body;

        body["stories"] = std::move(stories);

        return jsonResponse(200, body);
    });

    // POST STORIES ROUTE

    CROW_ROUTE(app, "/api/stories")
    .methods("POST"_method)(
        [](const crow::request& request) {

            if (!authorized(request))
                return textResponse(
                    401,
                    "Please log in again"
                );

            const auto json =
                crow::json::load(request.body);

            if (!json)
                return textResponse(
                    400,
                    "Invalid story data"
                );

            std::string imageData =
                stringField(json, "image");

            std::string videoData =
                stringField(json, "video");

            if (imageData.size() > 12 * 1024 * 1024 ||
                videoData.size() > 14 * 1024 * 1024)

                return textResponse(
                    413,
                    "File is too large"
                );

            std::string imagePath;
            std::string videoPath;

            if (!imageData.empty()) {
                imagePath = saveDataUrl(imageData, false);

                if (imagePath.empty())
                    return textResponse(
                        400,
                        "Unsupported image or image upload failed"
                    );
            }

            if (!videoData.empty()) {
                videoPath = saveDataUrl(videoData, true);

                if (videoPath.empty())
                    return textResponse(
                        400,
                        "Unsupported video or video upload failed"
                    );
            }

            sqlite3* database = openDb();

            if (!database)
                return textResponse(
                    500,
                    "Database unavailable"
                );

            sqlite3_stmt* statement = nullptr;

            const char* sql =
                "INSERT INTO stories("
                "title,category,country,date,story,image,video"
                ") VALUES(?,?,?,?,?,?,?)";

            if (sqlite3_prepare_v2(
                database,
                sql,
                -1,
                &statement,
                nullptr
            ) != SQLITE_OK) {

                sqlite3_close(database);

                return textResponse(
                    500,
                    "Could not prepare story save"
                );
            }

            const std::string fields[] = {
                stringField(json, "title"),
                stringField(json, "category"),
                stringField(json, "country"),
                stringField(json, "date"),
                stringField(json, "story"),
                imagePath,
                videoPath
            };

            for (int i = 0; i < 7; ++i) {
                sqlite3_bind_text(
                    statement,
                    i + 1,
                    fields[i].c_str(),
                    -1,
                    SQLITE_TRANSIENT
                );
            }

            const bool saved =
                sqlite3_step(statement) == SQLITE_DONE;

            sqlite3_finalize(statement);
            sqlite3_close(database);

            return textResponse(
                saved ? 201 : 500,
                saved ? "Published" : "Could not save story"
            );
        }
    );

    // DELETE STORY ROUTE

    CROW_ROUTE(app, "/api/stories/<int>")
    .methods("DELETE"_method)(
        [](const crow::request& request, int id) {

            if (!authorized(request))
                return textResponse(
                    401,
                    "Please log in again"
                );

            if (id <= 0)
                return textResponse(
                    400,
                    "Invalid story ID"
                );

            sqlite3* database = openDb();

            if (!database)
                return textResponse(
                    500,
                    "Database unavailable"
                );

            sqlite3_stmt* statement = nullptr;

            const char* sql =
                "DELETE FROM stories WHERE id=?";

            if (sqlite3_prepare_v2(
                database,
                sql,
                -1,
                &statement,
                nullptr
            ) != SQLITE_OK) {

                sqlite3_close(database);

                return textResponse(
                    500,
                    "Could not prepare story deletion"
                );
            }

            sqlite3_bind_int(statement, 1, id);

            const int stepResult =
                sqlite3_step(statement);

            const int affectedRows =
                sqlite3_changes(database);

            sqlite3_finalize(statement);
            sqlite3_close(database);

            if (stepResult != SQLITE_DONE)
                return textResponse(
                    500,
                    "Delete failed"
                );

            if (affectedRows == 0)
                return textResponse(
                    404,
                    "Story not found"
                );

            return textResponse(
                200,
                "Deleted successfully"
            );
        }
    );

    // UPLOADS ROUTE

    CROW_ROUTE(app, "/uploads/<string>")
    .methods("GET"_method)(
        [](const crow::request&, std::string name) {

            if (name.find("..") != std::string::npos ||
                name.find('/') != std::string::npos ||
                name.find('\\') != std::string::npos)

                return crow::response(403);

            std::ifstream file(
                mediaDir() + "/" + name,
                std::ios::binary
            );

            if (!file)
                return crow::response(404);

            std::ostringstream content;

            content << file.rdbuf();

            crow::response response(content.str());

            const auto dot = name.rfind('.');

            const std::string ext =
                dot == std::string::npos
                    ? ""
                    : name.substr(dot + 1);

            if (ext == "png")
                response.set_header("Content-Type", "image/png");

            else if (ext == "webp")
                response.set_header("Content-Type", "image/webp");

            else if (ext == "mp4")
                response.set_header("Content-Type", "video/mp4");

            else if (ext == "webm")
                response.set_header("Content-Type", "video/webm");

            else
                response.set_header("Content-Type", "image/jpeg");

            response.set_header(
                "Cache-Control",
                "public, max-age=86400"
            );

            return response;
        }
    );

    // LOGOUT ROUTE

    CROW_ROUTE(app, "/api/logout")
    .methods("POST"_method)(
        [](const crow::request& request) {

            const std::string header =
                request.get_header_value("Authorization");

            if (header.rfind("Bearer ", 0) == 0) {

                std::lock_guard<std::mutex> lock(sessionMutex);

                const std::string session =
                    header.substr(7);

                sessions.erase(
                    std::remove(
                        sessions.begin(),
                        sessions.end(),
                        session
                    ),
                    sessions.end()
                );
            }

            return textResponse(
                200,
                "Logged out"
            );
        }
    );

    // RENDER PORT

    const char* portValue = std::getenv("PORT");

    int port = 8080;

    if (portValue && *portValue) {
        try {
            port = std::stoi(portValue);
        }
        catch (...) {
            std::cerr << "Invalid PORT environment variable"
                      << std::endl;
            return 1;
        }
    }

    app.bindaddr("0.0.0.0")
       .port(port)
       .multithreaded()
       .run();

    return 0;
}

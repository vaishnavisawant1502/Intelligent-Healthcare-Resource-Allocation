/**
 * Database Implementation
 * Intelligent Healthcare Resource Allocation Simulator (C++17)
 */

#include "db.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

Database g_db;

Database::Database() {
    load_env_config();
}

void Database::load_env_config() {
    const char* env_host = std::getenv("DB_HOST");
    host = env_host ? env_host : "localhost";

    const char* env_user = std::getenv("DB_USER");
    user = env_user ? env_user : "root";

    const char* env_pass = std::getenv("DB_PASSWORD");
    password = env_pass ? env_pass : "";

    const char* env_db = std::getenv("DB_NAME");
    database_name = env_db ? env_db : "healthcare_simulator";

    const char* env_port = std::getenv("DB_PORT");
    port = env_port ? std::atoi(env_port) : 3306;
}

MYSQL* Database::get_connection(bool select_db, std::string* out_error) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        if (out_error) *out_error = "Failed to initialize MySQL client structure.";
        return nullptr;
    }

    // Set reconnect option
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    const char* db_to_select = select_db ? database_name.c_str() : nullptr;

    if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(),
                            db_to_select, port, nullptr, CLIENT_MULTI_STATEMENTS)) {
        if (out_error) {
            *out_error = mysql_error(conn);
        }
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

void Database::close_connection(MYSQL* conn) {
    if (conn) {
        mysql_close(conn);
    }
}

std::string Database::escape(MYSQL* conn, const std::string& str) {
    if (!conn || str.empty()) return "";
    std::vector<char> buffer(str.length() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data(), len);
}

bool Database::execute(MYSQL* conn, const std::string& sql, std::string* out_error) {
    if (!conn) {
        if (out_error) *out_error = "Null connection passed to execute.";
        return false;
    }

    if (mysql_query(conn, sql.c_str()) != 0) {
        if (out_error) *out_error = mysql_error(conn);
        return false;
    }

    // Clear any pending result sets if multi-statements are enabled
    MYSQL_RES* res = mysql_store_result(conn);
    if (res) mysql_free_result(res);
    while (mysql_next_result(conn) == 0) {
        res = mysql_store_result(conn);
        if (res) mysql_free_result(res);
    }

    return true;
}

unsigned long long Database::get_last_insert_id(MYSQL* conn) {
    return conn ? mysql_insert_id(conn) : 0;
}

my_ulonglong Database::get_affected_rows(MYSQL* conn) {
    return conn ? mysql_affected_rows(conn) : 0;
}

std::vector<std::map<std::string, std::string>> Database::query_rows(MYSQL* conn, const std::string& sql, std::string* out_error) {
    std::vector<std::map<std::string, std::string>> results;
    if (!conn) {
        if (out_error) *out_error = "Null connection passed to query_rows.";
        return results;
    }

    if (mysql_query(conn, sql.c_str()) != 0) {
        if (out_error) *out_error = mysql_error(conn);
        return results;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        if (mysql_field_count(conn) > 0 && out_error) {
            *out_error = mysql_error(conn);
        }
        return results;
    }

    int num_fields = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        std::map<std::string, std::string> row_map;
        unsigned long* lengths = mysql_fetch_lengths(res);
        for (int i = 0; i < num_fields; ++i) {
            std::string field_name = fields[i].name;
            if (row[i] != nullptr) {
                row_map[field_name] = std::string(row[i], lengths[i]);
            } else {
                row_map[field_name] = "";
            }
        }
        results.push_back(row_map);
    }

    mysql_free_result(res);

    // Consume any subsequent result sets
    while (mysql_next_result(conn) == 0) {
        res = mysql_store_result(conn);
        if (res) mysql_free_result(res);
    }

    return results;
}

bool Database::begin_transaction(MYSQL* conn) {
    return execute(conn, "START TRANSACTION;");
}

bool Database::commit(MYSQL* conn) {
    return execute(conn, "COMMIT;");
}

bool Database::rollback(MYSQL* conn) {
    return execute(conn, "ROLLBACK;");
}

json Database::check_health() {
    std::string err;
    MYSQL* conn = get_connection(true, &err);
    if (!conn) {
        return json{
            {"connected", false},
            {"error", "Failed to connect to MySQL database: " + err}
        };
    }

    std::vector<std::string> table_names;
    auto rows = query_rows(conn, "SHOW TABLES;", &err);
    for (const auto& r : rows) {
        if (!r.empty()) {
            table_names.push_back(r.begin()->second);
        }
    }

    close_connection(conn);

    return json{
        {"connected", true},
        {"database", database_name},
        {"tables", table_names}
    };
}

json Database::init_schema(const std::string& schema_path) {
    std::string resolved_path = schema_path;
    if (resolved_path.empty()) {
        resolved_path = "../database/schema.sql";
    }

    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        // Try absolute or current dir fallback
        file.open("database/schema.sql");
        if (!file.is_open()) {
            return json{
                {"success", false},
                {"error", "Schema file not found at: " + resolved_path}
            };
        }
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();
    file.close();

    std::string err;
    MYSQL* conn = get_connection(false, &err);
    if (!conn) {
        return json{
            {"success", false},
            {"error", "Could not connect to MySQL server: " + err}
        };
    }

    // Split script by semicolons
    std::stringstream ss(script);
    std::string statement;
    while (std::getline(ss, statement, ';')) {
        // Trim whitespace
        size_t first = statement.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) continue;
        size_t last = statement.find_last_not_of(" \t\n\r");
        std::string cmd = statement.substr(first, (last - first + 1));
        if (cmd.empty()) continue;

        if (!execute(conn, cmd, &err)) {
            close_connection(conn);
            return json{
                {"success", false},
                {"error", "Error executing SQL command: " + err}
            };
        }
    }

    close_connection(conn);
    return json{
        {"success", true},
        {"message", "Database and tables initialized successfully."}
    };
}

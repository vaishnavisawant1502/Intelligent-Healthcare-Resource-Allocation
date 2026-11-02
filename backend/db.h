/**
 * Database Connection & Query Utilities
 * Intelligent Healthcare Resource Allocation Simulator (C++17)
 * 
 * Provides safe MySQL connection management, SQL escaping to prevent SQL injection,
 * and query helper functions. Credentials are read from environment variables.
 */

#ifndef DB_H
#define DB_H

#include <string>
#include <vector>
#include <map>
#include <mysql.h>
#include "json.hpp"

using json = nlohmann::json;

class Database {
private:
    std::string host;
    std::string user;
    std::string password;
    std::string database_name;
    int port;

    void load_env_config();

public:
    Database();

    // Returns a new raw MYSQL connection (caller must close using close_connection)
    MYSQL* get_connection(bool select_db = true, std::string* out_error = nullptr);
    void close_connection(MYSQL* conn);

    // SQL string escaping to prevent SQL injection
    std::string escape(MYSQL* conn, const std::string& str);

    // Query execution helpers
    bool execute(MYSQL* conn, const std::string& sql, std::string* out_error = nullptr);
    unsigned long long get_last_insert_id(MYSQL* conn);
    my_ulonglong get_affected_rows(MYSQL* conn);

    // Query returning rows as vector of column_name -> value maps
    std::vector<std::map<std::string, std::string>> query_rows(MYSQL* conn, const std::string& sql, std::string* out_error = nullptr);

    // Transaction management
    bool begin_transaction(MYSQL* conn);
    bool commit(MYSQL* conn);
    bool rollback(MYSQL* conn);

    // Diagnostics & Schema bootstrap
    json check_health();
    json init_schema(const std::string& schema_path = "");
};

// Global database instance
extern Database g_db;

#endif // DB_H

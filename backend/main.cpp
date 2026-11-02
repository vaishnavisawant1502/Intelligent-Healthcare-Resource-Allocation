/**
 * Intelligent Healthcare Resource Allocation Simulator
 * C++17 Backend Server (cpp-httplib + nlohmann/json + MySQL C API)
 * 
 * Demonstrates:
 * - RESTful HTTP APIs matching the Python Flask backend 1:1
 * - Object-Oriented Programming (OOP) Domain Models
 * - Atomic MySQL transactions with row-level locking (SELECT ... FOR UPDATE)
 * - All-or-nothing resource allocation & safe release
 * - Static frontend delivery & CORS support
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "httplib.h"
#include "json.hpp"
#include "models.h"
#include "db.h"

using json = nlohmann::json;

// Instantiate domain staff objects for role logic, permissions & viva demonstration
Doctor active_doctor(101, "Sarah Jenkins", "Emergency Medicine", "Triage & Trauma");
Administrator active_admin(201, "Robert Vance", "Hospital Resource Operations");

// Helper to set CORS headers and standard JSON response
void set_json_response(httplib::Response& res, const json& data, int status_code = 200) {
    res.status = status_code;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_content(data.dump(), "application/json");
}

// Helper to send error response
void set_error_response(httplib::Response& res, const std::string& error_message, int status_code = 400) {
    json err = {
        {"success", false},
        {"error", error_message}
    };
    set_json_response(res, err, status_code);
}

// Helper to read static files from frontend directory
bool serve_static_file(const std::string& relative_path, const std::string& content_type, httplib::Response& res) {
    std::string full_path = "../frontend/" + relative_path;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        full_path = "frontend/" + relative_path;
        file.open(full_path, std::ios::binary);
    }
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    res.status = 200;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content(buffer.str(), content_type.c_str());
    return true;
}

int main() {
    httplib::Server svr;

    // -------------------------------------------------------------
    // CORS Preflight Options Handler
    // -------------------------------------------------------------
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // -------------------------------------------------------------
    // Static Frontend Serving Routes
    // -------------------------------------------------------------
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        if (!serve_static_file("index.html", "text/html", res)) {
            res.status = 404;
            res.set_content("index.html not found in frontend directory", "text/plain");
        }
    });

    svr.Get("/styles.css", [](const httplib::Request&, httplib::Response& res) {
        if (!serve_static_file("styles.css", "text/css", res)) {
            res.status = 404;
        }
    });

    svr.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        if (!serve_static_file("app.js", "application/javascript", res)) {
            res.status = 404;
        }
    });

    // -------------------------------------------------------------
    // Health and System Diagnostics
    // -------------------------------------------------------------
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        json db_health = g_db.check_health();
        bool is_healthy = db_health.value("connected", false);

        json response = {
            {"status", is_healthy ? "healthy" : "degraded"},
            {"database", db_health},
            {"doctor_profile", active_doctor.get_summary()},
            {"admin_profile", active_admin.get_summary()}
        };
        set_json_response(res, response, is_healthy ? 200 : 503);
    });

    svr.Post("/api/init-db", [](const httplib::Request&, httplib::Response& res) {
        json result = g_db.init_schema();
        bool success = result.value("success", false);
        set_json_response(res, result, success ? 200 : 500);
    });

    // -------------------------------------------------------------
    // Receptionist: Patient Registration & Directory
    // -------------------------------------------------------------
    svr.Post("/api/patients", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            set_error_response(res, "Malformed JSON request body.", 400);
            return;
        }

        std::string name = body.value("name", "");
        int age = body.value("age", 0);
        std::string gender = body.value("gender", "");
        std::string contact = body.value("contact", "");
        std::string reason = body.value("reason_for_visit", "");

        // Validation using Patient OOP model
        try {
            Patient patient(0, name, age, gender, contact, reason, "Awaiting Assessment");

            if (patient.get_name().empty()) {
                set_error_response(res, "Patient name is required.", 400);
                return;
            }
            if (patient.get_age() <= 0) {
                set_error_response(res, "Age must be a positive integer.", 400);
                return;
            }
            if (patient.get_gender().empty()) {
                set_error_response(res, "Gender is required.", 400);
                return;
            }
            if (patient.get_contact().empty()) {
                set_error_response(res, "Contact number is required.", 400);
                return;
            }
            if (patient.get_reason_for_visit().empty()) {
                set_error_response(res, "Reason for visit is required.", 400);
                return;
            }
        } catch (const std::exception& e) {
            set_error_response(res, e.what(), 400);
            return;
        }

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string esc_name = g_db.escape(conn, name);
        std::string esc_gender = g_db.escape(conn, gender);
        std::string esc_contact = g_db.escape(conn, contact);
        std::string esc_reason = g_db.escape(conn, reason);

        std::string sql = "INSERT INTO patients (name, age, gender, contact, reason_for_visit, status) VALUES ('" +
                          esc_name + "', " + std::to_string(age) + ", '" + esc_gender + "', '" +
                          esc_contact + "', '" + esc_reason + "', 'Awaiting Assessment');";

        if (!g_db.execute(conn, sql, &err)) {
            g_db.close_connection(conn);
            set_error_response(res, "Database error during patient registration: " + err, 500);
            return;
        }

        unsigned long long new_id = g_db.get_last_insert_id(conn);
        g_db.close_connection(conn);

        json success_res = {
            {"success", true},
            {"patient_id", new_id},
            {"message", "Patient '" + name + "' registered successfully."}
        };
        set_json_response(res, success_res, 201);
    });

    svr.Get("/api/patients", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string sql = R"(
            SELECT 
                p.id,
                p.name,
                p.age,
                p.gender,
                p.contact,
                p.reason_for_visit,
                p.status,
                p.created_at,
                da.id AS latest_assessment_id,
                da.doctor_name,
                da.urgency_level,
                da.examination_findings,
                da.assessment_status
            FROM patients p
            LEFT JOIN doctor_assessments da ON da.id = (
                SELECT id FROM doctor_assessments 
                WHERE patient_id = p.id 
                ORDER BY created_at DESC LIMIT 1
            )
            ORDER BY p.created_at DESC;
        )";

        auto rows = g_db.query_rows(conn, sql, &err);
        g_db.close_connection(conn);

        if (!err.empty()) {
            set_error_response(res, "Failed to fetch patients: " + err, 500);
            return;
        }

        json patient_list = json::array();
        for (const auto& r : rows) {
            json item;
            item["id"] = std::atoi(r.at("id").c_str());
            item["name"] = r.at("name");
            item["age"] = std::atoi(r.at("age").c_str());
            item["gender"] = r.at("gender");
            item["contact"] = r.at("contact");
            item["reason_for_visit"] = r.at("reason_for_visit");
            item["status"] = r.at("status");
            item["created_at"] = r.at("created_at");

            if (!r.at("latest_assessment_id").empty()) {
                item["latest_assessment_id"] = std::atoi(r.at("latest_assessment_id").c_str());
                item["doctor_name"] = r.at("doctor_name");
                item["urgency_level"] = r.at("urgency_level");
                item["examination_findings"] = r.at("examination_findings");
                item["assessment_status"] = r.at("assessment_status");
            } else {
                item["latest_assessment_id"] = nullptr;
                item["doctor_name"] = nullptr;
                item["urgency_level"] = nullptr;
                item["examination_findings"] = nullptr;
                item["assessment_status"] = nullptr;
            }
            patient_list.push_back(item);
        }

        json result = {
            {"success", true},
            {"patients", patient_list}
        };
        set_json_response(res, result, 200);
    });

    svr.Get(R"(/api/patients/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int patient_id = std::atoi(req.matches[1].str().c_str());

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        // Fetch patient
        std::string pat_sql = "SELECT * FROM patients WHERE id = " + std::to_string(patient_id) + ";";
        auto pat_rows = g_db.query_rows(conn, pat_sql, &err);
        if (pat_rows.empty()) {
            g_db.close_connection(conn);
            set_error_response(res, "Patient with ID " + std::to_string(patient_id) + " not found.", 404);
            return;
        }

        const auto& p_map = pat_rows[0];
        json patient_json = {
            {"id", std::atoi(p_map.at("id").c_str())},
            {"name", p_map.at("name")},
            {"age", std::atoi(p_map.at("age").c_str())},
            {"gender", p_map.at("gender")},
            {"contact", p_map.at("contact")},
            {"reason_for_visit", p_map.at("reason_for_visit")},
            {"status", p_map.at("status")},
            {"created_at", p_map.at("created_at")}
        };

        // Fetch assessments
        std::string assess_sql = "SELECT id, doctor_name, examination_findings, urgency_level, assessment_status, created_at "
                                 "FROM doctor_assessments WHERE patient_id = " + std::to_string(patient_id) +
                                 " ORDER BY created_at DESC;";
        auto assess_rows = g_db.query_rows(conn, assess_sql);

        json assessments_json = json::array();
        for (const auto& a : assess_rows) {
            json a_obj;
            int a_id = std::atoi(a.at("id").c_str());
            a_obj["id"] = a_id;
            a_obj["doctor_name"] = a.at("doctor_name");
            a_obj["examination_findings"] = a.at("examination_findings");
            a_obj["urgency_level"] = a.at("urgency_level");
            a_obj["assessment_status"] = a.at("assessment_status");
            a_obj["created_at"] = a.at("created_at");

            // Fetch requirements for assessment
            std::string req_sql = "SELECT arr.resource_id, r.name AS resource_name, arr.requested_quantity "
                                  "FROM assessment_resource_requirements arr "
                                  "JOIN resources r ON arr.resource_id = r.id "
                                  "WHERE arr.assessment_id = " + std::to_string(a_id) + ";";
            auto req_rows = g_db.query_rows(conn, req_sql);
            json requirements = json::array();
            for (const auto& r : req_rows) {
                requirements.push_back({
                    {"resource_id", std::atoi(r.at("resource_id").c_str())},
                    {"resource_name", r.at("resource_name")},
                    {"requested_quantity", std::atoi(r.at("requested_quantity").c_str())}
                });
            }
            a_obj["requirements"] = requirements;
            assessments_json.push_back(a_obj);
        }

        // Fetch allocations
        std::string alloc_sql = "SELECT ra.id, ra.resource_id, r.name AS resource_name, ra.allocated_quantity, "
                                "ra.status, ra.allocated_at, ra.released_at "
                                "FROM resource_allocations ra "
                                "JOIN resources r ON ra.resource_id = r.id "
                                "WHERE ra.patient_id = " + std::to_string(patient_id) +
                                " ORDER BY ra.allocated_at DESC;";
        auto alloc_rows = g_db.query_rows(conn, alloc_sql);
        json allocations_json = json::array();
        for (const auto& al : alloc_rows) {
            allocations_json.push_back({
                {"id", std::atoi(al.at("id").c_str())},
                {"resource_id", std::atoi(al.at("resource_id").c_str())},
                {"resource_name", al.at("resource_name")},
                {"allocated_quantity", std::atoi(al.at("allocated_quantity").c_str())},
                {"status", al.at("status")},
                {"allocated_at", al.at("allocated_at")},
                {"released_at", al.at("released_at").empty() ? nullptr : json(al.at("released_at"))}
            });
        }

        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"patient", patient_json},
            {"assessments", assessments_json},
            {"allocations", allocations_json}
        };
        set_json_response(res, response, 200);
    });

    svr.Get("/api/patients/awaiting-assessment", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string sql = "SELECT id, name, age, gender, contact, reason_for_visit, status, created_at "
                          "FROM patients WHERE status IN ('Registered', 'Awaiting Assessment') "
                          "ORDER BY created_at ASC;";
        auto rows = g_db.query_rows(conn, sql, &err);
        g_db.close_connection(conn);

        if (!err.empty()) {
            set_error_response(res, "Failed to fetch awaiting patients: " + err, 500);
            return;
        }

        json patient_list = json::array();
        for (const auto& r : rows) {
            patient_list.push_back({
                {"id", std::atoi(r.at("id").c_str())},
                {"name", r.at("name")},
                {"age", std::atoi(r.at("age").c_str())},
                {"gender", r.at("gender")},
                {"contact", r.at("contact")},
                {"reason_for_visit", r.at("reason_for_visit")},
                {"status", r.at("status")},
                {"created_at", r.at("created_at")}
            });
        }

        json response = {
            {"success", true},
            {"patients", patient_list}
        };
        set_json_response(res, response, 200);
    });

    // -------------------------------------------------------------
    // Doctor: Clinical Assessment & All-or-Nothing Resource Allocation
    // -------------------------------------------------------------
    svr.Post("/api/assessments", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            set_error_response(res, "Malformed JSON request body.", 400);
            return;
        }

        if (!body.contains("patient_id") || body["patient_id"].is_null()) {
            set_error_response(res, "Patient ID is required.", 400);
            return;
        }

        int patient_id = body["patient_id"].get<int>();
        std::string doctor_name = body.value("doctor_name", active_doctor.get_name());
        std::string findings = body.value("examination_findings", "");
        std::string urgency = body.value("urgency_level", "Medium");

        // Parse requested resources map {resource_id: quantity}
        std::map<int, int> parsed_requirements;
        if (body.contains("requested_resources") && body["requested_resources"].is_object()) {
            for (auto it = body["requested_resources"].begin(); it != body["requested_resources"].end(); ++it) {
                try {
                    int res_id = std::stoi(it.key());
                    int qty = it.value().is_number() ? it.value().get<int>() : std::stoi(it.value().get<std::string>());
                    if (qty > 0) {
                        parsed_requirements[res_id] = qty;
                    } else if (qty < 0) {
                        set_error_response(res, "Resource quantities must be non-negative integers.", 400);
                        return;
                    }
                } catch (...) {
                    set_error_response(res, "Resource quantities must be valid integers.", 400);
                    return;
                }
            }
        }

        // Validate assessment input using Doctor OOP model
        std::string validation_error;
        if (!active_doctor.validate_assessment_input(findings, urgency, parsed_requirements, validation_error)) {
            set_error_response(res, validation_error, 400);
            return;
        }

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        // Start single atomic transaction
        g_db.begin_transaction(conn);

        // 1. Lock patient row
        std::string lock_patient_sql = "SELECT id, name, status FROM patients WHERE id = " + std::to_string(patient_id) + " FOR UPDATE;";
        auto p_rows = g_db.query_rows(conn, lock_patient_sql, &err);
        if (p_rows.empty()) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Patient with ID " + std::to_string(patient_id) + " does not exist.", 404);
            return;
        }

        // 2. Insert assessment into doctor_assessments
        std::string esc_doc = g_db.escape(conn, doctor_name);
        std::string esc_findings = g_db.escape(conn, findings);
        std::string esc_urgency = g_db.escape(conn, urgency);

        std::string insert_assess_sql = "INSERT INTO doctor_assessments (patient_id, doctor_name, examination_findings, urgency_level, assessment_status) "
                                        "VALUES (" + std::to_string(patient_id) + ", '" + esc_doc + "', '" +
                                        esc_findings + "', '" + esc_urgency + "', 'Pending');";
        if (!g_db.execute(conn, insert_assess_sql, &err)) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Failed to record doctor assessment: " + err, 500);
            return;
        }
        unsigned long long assessment_id = g_db.get_last_insert_id(conn);

        // 3. Insert requirements into assessment_resource_requirements
        for (const auto& pair : parsed_requirements) {
            std::string ins_req_sql = "INSERT INTO assessment_resource_requirements (assessment_id, resource_id, requested_quantity) "
                                      "VALUES (" + std::to_string(assessment_id) + ", " +
                                      std::to_string(pair.first) + ", " + std::to_string(pair.second) + ");";
            g_db.execute(conn, ins_req_sql);
        }

        // 4. Special Case: If doctor requested zero resources, complete assessment without allocations
        if (parsed_requirements.empty()) {
            g_db.execute(conn, "UPDATE doctor_assessments SET assessment_status = 'Assessed' WHERE id = " + std::to_string(assessment_id) + ";");
            g_db.execute(conn, "UPDATE patients SET status = 'Assessed' WHERE id = " + std::to_string(patient_id) + ";");
            g_db.commit(conn);
            g_db.close_connection(conn);

            json res_zero = {
                {"success", true},
                {"allocated", false},
                {"assessment_id", assessment_id},
                {"message", "Assessment submitted successfully. No resources requested."}
            };
            set_json_response(res, res_zero, 200);
            return;
        }

        // 5. Lock resource rows in sorted ID order to avoid deadlocks
        std::vector<int> sorted_ids;
        for (const auto& pair : parsed_requirements) {
            sorted_ids.push_back(pair.first);
        }
        std::sort(sorted_ids.begin(), sorted_ids.end());

        std::stringstream ids_ss;
        for (size_t i = 0; i < sorted_ids.size(); ++i) {
            if (i > 0) ids_ss << ",";
            ids_ss << sorted_ids[i];
        }

        std::string lock_res_sql = "SELECT id, resource_type, name, total_quantity, allocated_quantity, available_quantity "
                                   "FROM resources WHERE id IN (" + ids_ss.str() + ") FOR UPDATE;";
        auto locked_rows = g_db.query_rows(conn, lock_res_sql, &err);
        std::map<int, std::map<std::string, std::string>> locked_resources;
        for (const auto& r : locked_rows) {
            locked_resources[std::atoi(r.at("id").c_str())] = r;
        }

        // Validate all requested resource IDs exist
        for (int r_id : sorted_ids) {
            if (locked_resources.find(r_id) == locked_resources.end()) {
                g_db.rollback(conn);
                g_db.close_connection(conn);
                set_error_response(res, "Resource ID " + std::to_string(r_id) + " does not exist in inventory.", 400);
                return;
            }
        }

        // 6. All-or-Nothing Availability Check
        bool can_allocate_all = true;
        std::vector<std::string> shortage_reasons;

        for (int r_id : sorted_ids) {
            const auto& res_map = locked_resources[r_id];
            int req_qty = parsed_requirements[r_id];
            int avail = std::atoi(res_map.at("available_quantity").c_str());

            if (avail < req_qty) {
                can_allocate_all = false;
                shortage_reasons.push_back(
                    res_map.at("name") + " (Requested: " + std::to_string(req_qty) + ", Available: " + std::to_string(avail) + ")"
                );
            }
        }

        if (can_allocate_all) {
            // 7A. ALL AVAILABLE: Deduct inventory and record allocations
            for (int r_id : sorted_ids) {
                int req_qty = parsed_requirements[r_id];

                std::string update_inv_sql = "UPDATE resources SET allocated_quantity = allocated_quantity + " +
                                             std::to_string(req_qty) + " WHERE id = " + std::to_string(r_id) + ";";
                g_db.execute(conn, update_inv_sql);

                std::string ins_alloc_sql = "INSERT INTO resource_allocations (patient_id, assessment_id, resource_id, allocated_quantity, status) "
                                            "VALUES (" + std::to_string(patient_id) + ", " +
                                            std::to_string(assessment_id) + ", " +
                                            std::to_string(r_id) + ", " +
                                            std::to_string(req_qty) + ", 'Allocated');";
                g_db.execute(conn, ins_alloc_sql);
            }

            g_db.execute(conn, "UPDATE doctor_assessments SET assessment_status = 'Allocated' WHERE id = " + std::to_string(assessment_id) + ";");
            g_db.execute(conn, "UPDATE patients SET status = 'Resources Allocated' WHERE id = " + std::to_string(patient_id) + ";");

            g_db.commit(conn);
            g_db.close_connection(conn);

            json alloc_res = {
                {"success", true},
                {"allocated", true},
                {"assessment_id", assessment_id},
                {"message", "All requested resources were successfully allocated."}
            };
            set_json_response(res, alloc_res, 201);
        } else {
            // 7B. SHORTAGE: Allocate NOTHING (All-or-Nothing rule). Mark as Waiting for Resources.
            g_db.execute(conn, "UPDATE doctor_assessments SET assessment_status = 'Waiting for Resources' WHERE id = " + std::to_string(assessment_id) + ";");
            g_db.execute(conn, "UPDATE patients SET status = 'Waiting for Resources' WHERE id = " + std::to_string(patient_id) + ";");

            g_db.commit(conn);
            g_db.close_connection(conn);

            std::stringstream reasons_ss;
            for (size_t i = 0; i < shortage_reasons.size(); ++i) {
                if (i > 0) reasons_ss << "; ";
                reasons_ss << shortage_reasons[i];
            }

            json wait_res = {
                {"success", true},
                {"allocated", false},
                {"assessment_id", assessment_id},
                {"shortages", shortage_reasons},
                {"message", "Shortage detected: " + reasons_ss.str() + ". Request placed in Waiting for Resources queue."}
            };
            set_json_response(res, wait_res, 200);
        }
    });

    // -------------------------------------------------------------
    // Administrator: Inventory Management
    // -------------------------------------------------------------
    svr.Get("/api/resources", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string sql = "SELECT id, resource_type, name, total_quantity, allocated_quantity, available_quantity "
                          "FROM resources ORDER BY id ASC;";
        auto rows = g_db.query_rows(conn, sql, &err);
        g_db.close_connection(conn);

        if (!err.empty()) {
            set_error_response(res, "Failed to retrieve resources: " + err, 500);
            return;
        }

        json resource_list = json::array();
        for (const auto& r : rows) {
            resource_list.push_back({
                {"id", std::atoi(r.at("id").c_str())},
                {"resource_type", r.at("resource_type")},
                {"name", r.at("name")},
                {"total_quantity", std::atoi(r.at("total_quantity").c_str())},
                {"allocated_quantity", std::atoi(r.at("allocated_quantity").c_str())},
                {"available_quantity", std::atoi(r.at("available_quantity").c_str())}
            });
        }

        json response = {
            {"success", true},
            {"resources", resource_list}
        };
        set_json_response(res, response, 200);
    });

    svr.Post("/api/resources/update", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            set_error_response(res, "Malformed JSON body.", 400);
            return;
        }

        if (!body.contains("resource_id") || !body.contains("total_quantity")) {
            set_error_response(res, "Both resource_id and total_quantity are required.", 400);
            return;
        }

        int resource_id = body["resource_id"].get<int>();
        int new_total = body["total_quantity"].get<int>();

        if (new_total < 0) {
            set_error_response(res, "Total inventory quantity cannot be negative.", 400);
            return;
        }

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        g_db.begin_transaction(conn);

        // Lock row FOR UPDATE
        std::string lock_sql = "SELECT id, name, total_quantity, allocated_quantity FROM resources WHERE id = " +
                               std::to_string(resource_id) + " FOR UPDATE;";
        auto rows = g_db.query_rows(conn, lock_sql, &err);
        if (rows.empty()) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Resource with ID " + std::to_string(resource_id) + " not found.", 404);
            return;
        }

        int current_allocated = std::atoi(rows[0].at("allocated_quantity").c_str());
        std::string res_name = rows[0].at("name");

        // Validate using Administrator OOP domain method
        std::string validation_error;
        if (!active_admin.validate_inventory_update(current_allocated, new_total, validation_error)) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, validation_error, 400);
            return;
        }

        std::string update_sql = "UPDATE resources SET total_quantity = " + std::to_string(new_total) +
                                 " WHERE id = " + std::to_string(resource_id) + ";";
        if (!g_db.execute(conn, update_sql, &err)) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Database error updating inventory: " + err, 500);
            return;
        }

        g_db.commit(conn);
        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"message", "Successfully updated total capacity for '" + res_name + "' to " + std::to_string(new_total) + "."}
        };
        set_json_response(res, response, 200);
    });

    // -------------------------------------------------------------
    // Administrator: Resource Release (Preventing Duplicate Releases)
    // -------------------------------------------------------------
    svr.Get("/api/allocations", [](const httplib::Request& req, httplib::Response& res) {
        std::string status_filter = req.get_param_value("status");

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string sql = R"(
            SELECT 
                ra.id AS allocation_id,
                ra.patient_id,
                p.name AS patient_name,
                ra.assessment_id,
                ra.resource_id,
                r.name AS resource_name,
                ra.allocated_quantity,
                ra.status,
                ra.allocated_at,
                ra.released_at
            FROM resource_allocations ra
            JOIN patients p ON ra.patient_id = p.id
            JOIN resources r ON ra.resource_id = r.id
        )";

        if (!status_filter.empty()) {
            sql += " WHERE ra.status = '" + g_db.escape(conn, status_filter) + "'";
        }
        sql += " ORDER BY ra.allocated_at DESC;";

        auto rows = g_db.query_rows(conn, sql, &err);
        g_db.close_connection(conn);

        if (!err.empty()) {
            set_error_response(res, "Failed to retrieve allocations: " + err, 500);
            return;
        }

        json alloc_list = json::array();
        for (const auto& a : rows) {
            alloc_list.push_back({
                {"allocation_id", std::atoi(a.at("allocation_id").c_str())},
                {"patient_id", std::atoi(a.at("patient_id").c_str())},
                {"patient_name", a.at("patient_name")},
                {"assessment_id", std::atoi(a.at("assessment_id").c_str())},
                {"resource_id", std::atoi(a.at("resource_id").c_str())},
                {"resource_name", a.at("resource_name")},
                {"allocated_quantity", std::atoi(a.at("allocated_quantity").c_str())},
                {"status", a.at("status")},
                {"allocated_at", a.at("allocated_at")},
                {"released_at", a.at("released_at").empty() ? nullptr : json(a.at("released_at"))}
            });
        }

        json response = {
            {"success", true},
            {"allocations", alloc_list}
        };
        set_json_response(res, response, 200);
    });

    svr.Post(R"(/api/allocations/(\d+)/release)", [](const httplib::Request& req, httplib::Response& res) {
        int allocation_id = std::atoi(req.matches[1].str().c_str());

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        g_db.begin_transaction(conn);

        // 1. Lock allocation record FOR UPDATE
        std::string lock_alloc_sql = "SELECT id, patient_id, resource_id, allocated_quantity, status "
                                     "FROM resource_allocations WHERE id = " + std::to_string(allocation_id) + " FOR UPDATE;";
        auto alloc_rows = g_db.query_rows(conn, lock_alloc_sql, &err);
        if (alloc_rows.empty()) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Allocation record #" + std::to_string(allocation_id) + " not found.", 404);
            return;
        }

        const auto& alloc_row = alloc_rows[0];
        std::string alloc_status = alloc_row.at("status");
        int patient_id = std::atoi(alloc_row.at("patient_id").c_str());
        int resource_id = std::atoi(alloc_row.at("resource_id").c_str());
        int release_qty = std::atoi(alloc_row.at("allocated_quantity").c_str());

        // 2. Prevent duplicate release using Allocation OOP logic
        if (alloc_status != "Allocated") {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "Allocation #" + std::to_string(allocation_id) + " has already been " + alloc_status + ". Duplicate release prevented.", 400);
            return;
        }

        // 3. Lock resource row and restore inventory
        std::string lock_res_sql = "SELECT id, name, allocated_quantity FROM resources WHERE id = " + std::to_string(resource_id) + " FOR UPDATE;";
        auto res_rows = g_db.query_rows(conn, lock_res_sql);
        std::string res_name = res_rows.empty() ? "Resource" : res_rows[0].at("name");

        std::string update_res_sql = "UPDATE resources SET allocated_quantity = allocated_quantity - " +
                                     std::to_string(release_qty) + " WHERE id = " + std::to_string(resource_id) + ";";
        g_db.execute(conn, update_res_sql);

        // 4. Mark allocation as Released
        std::string update_alloc_sql = "UPDATE resource_allocations SET status = 'Released', released_at = NOW() WHERE id = " +
                                       std::to_string(allocation_id) + ";";
        g_db.execute(conn, update_alloc_sql);

        // 5. Check if patient has any remaining active allocations; if 0, mark Discharged
        std::string count_sql = "SELECT COUNT(*) AS active_count FROM resource_allocations WHERE patient_id = " +
                                std::to_string(patient_id) + " AND status = 'Allocated';";
        auto count_rows = g_db.query_rows(conn, count_sql);
        int remaining = count_rows.empty() ? 0 : std::atoi(count_rows[0].at("active_count").c_str());
        if (remaining == 0) {
            g_db.execute(conn, "UPDATE patients SET status = 'Discharged' WHERE id = " + std::to_string(patient_id) + ";");
        }

        g_db.commit(conn);
        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"message", "Successfully released " + std::to_string(release_qty) + " units of '" + res_name + "'. Inventory restored."}
        };
        set_json_response(res, response, 200);
    });

    svr.Post(R"(/api/patients/(\d+)/release-all)", [](const httplib::Request& req, httplib::Response& res) {
        int patient_id = std::atoi(req.matches[1].str().c_str());

        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        g_db.begin_transaction(conn);

        // 1. Lock all active allocations for the patient
        std::string sql = "SELECT ra.id, ra.resource_id, r.name AS resource_name, ra.allocated_quantity "
                          "FROM resource_allocations ra "
                          "JOIN resources r ON ra.resource_id = r.id "
                          "WHERE ra.patient_id = " + std::to_string(patient_id) + " AND ra.status = 'Allocated' "
                          "FOR UPDATE;";
        auto active_allocs = g_db.query_rows(conn, sql, &err);
        if (active_allocs.empty()) {
            g_db.rollback(conn);
            g_db.close_connection(conn);
            set_error_response(res, "No active allocations found for patient #" + std::to_string(patient_id) + ".", 400);
            return;
        }

        // 2. Release each allocation and restore inventory
        for (const auto& a : active_allocs) {
            int alloc_id = std::atoi(a.at("id").c_str());
            int res_id = std::atoi(a.at("resource_id").c_str());
            int alloc_qty = std::atoi(a.at("allocated_quantity").c_str());

            std::string update_inv = "UPDATE resources SET allocated_quantity = allocated_quantity - " +
                                     std::to_string(alloc_qty) + " WHERE id = " + std::to_string(res_id) + ";";
            g_db.execute(conn, update_inv);

            std::string update_alloc = "UPDATE resource_allocations SET status = 'Released', released_at = NOW() WHERE id = " +
                                       std::to_string(alloc_id) + ";";
            g_db.execute(conn, update_alloc);
        }

        // 3. Mark patient Discharged
        g_db.execute(conn, "UPDATE patients SET status = 'Discharged' WHERE id = " + std::to_string(patient_id) + ";");

        g_db.commit(conn);
        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"released_count", active_allocs.size()},
            {"message", "Released " + std::to_string(active_allocs.size()) + " active resource allocations for patient #" +
                        std::to_string(patient_id) + ". Patient discharged."}
        };
        set_json_response(res, response, 200);
    });

    // -------------------------------------------------------------
    // Administrator: Urgency-Prioritized Waiting Queue & Processing
    // -------------------------------------------------------------
    svr.Get("/api/waiting-list", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        std::string sql = R"(
            SELECT 
                a.id AS assessment_id,
                p.id AS patient_id,
                p.name AS patient_name,
                p.age,
                p.gender,
                p.contact,
                p.reason_for_visit,
                a.doctor_name,
                a.examination_findings,
                a.urgency_level,
                a.created_at AS assessment_time,
                p.created_at AS registration_time
            FROM doctor_assessments a
            JOIN patients p ON a.patient_id = p.id
            WHERE a.assessment_status = 'Waiting for Resources' AND p.status = 'Waiting for Resources'
            ORDER BY 
                CASE a.urgency_level
                    WHEN 'Critical' THEN 1
                    WHEN 'High' THEN 2
                    WHEN 'Medium' THEN 3
                    WHEN 'Low' THEN 4
                    ELSE 5
                END ASC,
                a.created_at ASC;
        )";

        auto rows = g_db.query_rows(conn, sql, &err);
        if (!err.empty()) {
            g_db.close_connection(conn);
            set_error_response(res, "Failed to retrieve waiting queue: " + err, 500);
            return;
        }

        json waiting_queue = json::array();
        for (const auto& req_entry : rows) {
            json item;
            int assess_id = std::atoi(req_entry.at("assessment_id").c_str());
            item["assessment_id"] = assess_id;
            item["patient_id"] = std::atoi(req_entry.at("patient_id").c_str());
            item["patient_name"] = req_entry.at("patient_name");
            item["age"] = std::atoi(req_entry.at("age").c_str());
            item["gender"] = req_entry.at("gender");
            item["contact"] = req_entry.at("contact");
            item["reason_for_visit"] = req_entry.at("reason_for_visit");
            item["doctor_name"] = req_entry.at("doctor_name");
            item["examination_findings"] = req_entry.at("examination_findings");
            item["urgency_level"] = req_entry.at("urgency_level");
            item["assessment_time"] = req_entry.at("assessment_time");
            item["registration_time"] = req_entry.at("registration_time");

            // Attach requested requirements with current inventory availability
            std::string req_sql = "SELECT arr.resource_id, r.name AS resource_name, arr.requested_quantity, r.available_quantity "
                                  "FROM assessment_resource_requirements arr "
                                  "JOIN resources r ON arr.resource_id = r.id "
                                  "WHERE arr.assessment_id = " + std::to_string(assess_id) + ";";
            auto req_rows = g_db.query_rows(conn, req_sql);
            json requirements = json::array();
            for (const auto& r : req_rows) {
                requirements.push_back({
                    {"resource_id", std::atoi(r.at("resource_id").c_str())},
                    {"resource_name", r.at("resource_name")},
                    {"requested_quantity", std::atoi(r.at("requested_quantity").c_str())},
                    {"available_quantity", std::atoi(r.at("available_quantity").c_str())}
                });
            }
            item["requirements"] = requirements;
            waiting_queue.push_back(item);
        }

        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"simulation_notice", "Queue ranking (Critical > High > Medium > Low) is an educational simulation mechanic and does not constitute clinical triage."},
            {"waiting_queue", waiting_queue}
        };
        set_json_response(res, response, 200);
    });

    svr.Post("/api/waiting-list/process", [](const httplib::Request&, httplib::Response& res) {
        std::string err;
        MYSQL* conn = g_db.get_connection(true, &err);
        if (!conn) {
            set_error_response(res, "Database connection failed: " + err, 500);
            return;
        }

        g_db.begin_transaction(conn);

        // Fetch waiting assessments in prioritized order
        std::string sql = R"(
            SELECT a.id AS assessment_id, a.patient_id, p.name AS patient_name, a.urgency_level
            FROM doctor_assessments a
            JOIN patients p ON a.patient_id = p.id
            WHERE a.assessment_status = 'Waiting for Resources' AND p.status = 'Waiting for Resources'
            ORDER BY 
                CASE a.urgency_level
                    WHEN 'Critical' THEN 1
                    WHEN 'High' THEN 2
                    WHEN 'Medium' THEN 3
                    WHEN 'Low' THEN 4
                    ELSE 5
                END ASC,
                a.created_at ASC
            FOR UPDATE;
        )";

        auto waiting_entries = g_db.query_rows(conn, sql, &err);

        json allocated_results = json::array();
        json still_waiting_results = json::array();

        for (const auto& entry : waiting_entries) {
            int assessment_id = std::atoi(entry.at("assessment_id").c_str());
            int patient_id = std::atoi(entry.at("patient_id").c_str());
            std::string patient_name = entry.at("patient_name");
            std::string urgency = entry.at("urgency_level");

            // Fetch requirements for assessment
            std::string req_sql = "SELECT arr.resource_id, r.name AS resource_name, arr.requested_quantity "
                                  "FROM assessment_resource_requirements arr "
                                  "JOIN resources r ON arr.resource_id = r.id "
                                  "WHERE arr.assessment_id = " + std::to_string(assessment_id) + ";";
            auto requirements = g_db.query_rows(conn, req_sql);

            bool can_allocate = true;
            std::vector<std::string> shortage_details;

            for (const auto& req : requirements) {
                int r_id = std::atoi(req.at("resource_id").c_str());
                int req_qty = std::atoi(req.at("requested_quantity").c_str());

                // Refresh current available quantity with row lock
                std::string chk_sql = "SELECT id, name, available_quantity FROM resources WHERE id = " +
                                      std::to_string(r_id) + " FOR UPDATE;";
                auto chk_rows = g_db.query_rows(conn, chk_sql);
                int cur_avail = chk_rows.empty() ? 0 : std::atoi(chk_rows[0].at("available_quantity").c_str());

                if (chk_rows.empty() || cur_avail < req_qty) {
                    can_allocate = false;
                    shortage_details.push_back(
                        req.at("resource_name") + " (Needed: " + std::to_string(req_qty) + ", Available: " + std::to_string(cur_avail) + ")"
                    );
                }
            }

            if (can_allocate && !requirements.empty()) {
                // Fulfill atomically
                for (const auto& req : requirements) {
                    int r_id = std::atoi(req.at("resource_id").c_str());
                    int req_qty = std::atoi(req.at("requested_quantity").c_str());

                    std::string upd_inv = "UPDATE resources SET allocated_quantity = allocated_quantity + " +
                                          std::to_string(req_qty) + " WHERE id = " + std::to_string(r_id) + ";";
                    g_db.execute(conn, upd_inv);

                    std::string ins_alloc = "INSERT INTO resource_allocations (patient_id, assessment_id, resource_id, allocated_quantity, status) "
                                            "VALUES (" + std::to_string(patient_id) + ", " +
                                            std::to_string(assessment_id) + ", " +
                                            std::to_string(r_id) + ", " +
                                            std::to_string(req_qty) + ", 'Allocated');";
                    g_db.execute(conn, ins_alloc);
                }

                g_db.execute(conn, "UPDATE doctor_assessments SET assessment_status = 'Allocated' WHERE id = " + std::to_string(assessment_id) + ";");
                g_db.execute(conn, "UPDATE patients SET status = 'Resources Allocated' WHERE id = " + std::to_string(patient_id) + ";");

                allocated_results.push_back({
                    {"patient_id", patient_id},
                    {"patient_name", patient_name},
                    {"urgency", urgency},
                    {"status", "Allocated"}
                });
            } else {
                still_waiting_results.push_back({
                    {"patient_id", patient_id},
                    {"patient_name", patient_name},
                    {"urgency", urgency},
                    {"reasons", shortage_details}
                });
            }
        }

        g_db.commit(conn);
        g_db.close_connection(conn);

        json response = {
            {"success", true},
            {"total_evaluated", waiting_entries.size()},
            {"allocated_count", allocated_results.size()},
            {"still_waiting_count", still_waiting_results.size()},
            {"allocated", allocated_results},
            {"still_waiting", still_waiting_results},
            {"message", "Processed queue: " + std::to_string(allocated_results.size()) + " allocated, " +
                        std::to_string(still_waiting_results.size()) + " still waiting."}
        };
        set_json_response(res, response, 200);
    });

    // -------------------------------------------------------------
    // Start Server
    // -------------------------------------------------------------
    int port = 5000;
    const char* env_port = std::getenv("PORT");
    if (env_port) {
        port = std::atoi(env_port);
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " Intelligent Healthcare Resource Allocation Simulator (C++17)  " << std::endl;
    std::cout << " Server listening on http://0.0.0.0:" << port << std::endl;
    std::cout << "=================================================================" << std::endl;

    svr.listen("0.0.0.0", port);

    return 0;
}

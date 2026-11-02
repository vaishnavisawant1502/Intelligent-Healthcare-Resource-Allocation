/**
 * Object-Oriented Programming (OOP) Domain Models
 * Intelligent Healthcare Resource Allocation Simulator (C++17)
 * 
 * Demonstrates core OOP principles for college project viva:
 * 1. Classes & Objects: Patient, HospitalStaff, Doctor, Administrator, Resource, Allocation
 * 2. Constructors: Explicit constructors with parameter validation
 * 3. Encapsulation: Private/protected member variables accessed through validated getters/setters
 * 4. Inheritance: Doctor and Administrator inherit from HospitalStaff
 * 5. Abstraction: HospitalStaff is an Abstract Base Class with pure virtual functions (= 0)
 * 6. Polymorphism & Method Overriding: Doctor and Administrator override get_role_description() and get_permissions()
 */

#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;

// ============================================================================
// 1. Patient Class (Demonstrates Class, Object, Encapsulation)
// ============================================================================
class Patient {
private:
    int patient_id;
    std::string name;
    int age;
    std::string gender;
    std::string contact;
    std::string reason_for_visit;
    std::string _status;
    std::string created_at;

    static const std::set<std::string>& valid_statuses() {
        static const std::set<std::string> statuses = {
            "Registered",
            "Awaiting Assessment",
            "Assessed",
            "Resources Allocated",
            "Waiting for Resources",
            "Discharged"
        };
        return statuses;
    }

public:
    Patient(int id = 0,
            const std::string& n = "",
            int a = 0,
            const std::string& g = "",
            const std::string& c = "",
            const std::string& reason = "",
            const std::string& status = "Awaiting Assessment",
            const std::string& created = "")
        : patient_id(id), name(n), age(a), gender(g), contact(c), reason_for_visit(reason), created_at(created) {
        if (age < 0) {
            throw std::invalid_argument("Patient age cannot be negative.");
        }
        if (valid_statuses().count(status)) {
            _status = status;
        } else {
            _status = "Awaiting Assessment";
        }
    }

    // Getters (Encapsulation)
    int get_id() const { return patient_id; }
    const std::string& get_name() const { return name; }
    int get_age() const { return age; }
    const std::string& get_gender() const { return gender; }
    const std::string& get_contact() const { return contact; }
    const std::string& get_reason_for_visit() const { return reason_for_visit; }
    const std::string& get_status() const { return _status; }
    const std::string& get_created_at() const { return created_at; }

    // Setter with business rule validation
    void set_status(const std::string& new_status) {
        if (!valid_statuses().count(new_status)) {
            throw std::invalid_argument("Invalid patient status: " + new_status);
        }
        _status = new_status;
    }

    json to_json() const {
        return json{
            {"id", patient_id},
            {"name", name},
            {"age", age},
            {"gender", gender},
            {"contact", contact},
            {"reason_for_visit", reason_for_visit},
            {"status", _status},
            {"created_at", created_at.empty() ? nullptr : json(created_at)}
        };
    }
};

// ============================================================================
// 2. HospitalStaff Abstract Base Class (Demonstrates Abstraction & Inheritance)
// ============================================================================
class HospitalStaff {
protected:
    int _staff_id;
    std::string _name;
    std::string _department;
    std::string _role;

public:
    HospitalStaff(int staff_id, const std::string& name, const std::string& department, const std::string& role)
        : _staff_id(staff_id), _name(name), _department(department), _role(role) {}

    virtual ~HospitalStaff() = default;

    int get_staff_id() const { return _staff_id; }
    const std::string& get_name() const { return _name; }
    const std::string& get_department() const { return _department; }
    const std::string& get_role() const { return _role; }

    // Pure virtual functions (Abstraction): concrete subclasses MUST implement these
    virtual std::string get_role_description() const = 0;
    virtual std::vector<std::string> get_permissions() const = 0;

    // Concrete method shared by all subclasses
    virtual json get_summary() const {
        return json{
            {"staff_id", _staff_id},
            {"name", _name},
            {"department", _department},
            {"role", _role},
            {"description", get_role_description()},
            {"permissions", get_permissions()}
        };
    }
};

// ============================================================================
// 3. Doctor Class (Demonstrates Inheritance & Polymorphism / Method Overriding)
// ============================================================================
class Doctor : public HospitalStaff {
private:
    std::string _specialization;

public:
    Doctor(int staff_id, const std::string& name,
           const std::string& department = "Clinical Medicine",
           const std::string& specialization = "General Physician")
        : HospitalStaff(staff_id, name, department, "Doctor"), _specialization(specialization) {}

    const std::string& get_specialization() const { return _specialization; }

    // Method Overriding (Polymorphism)
    std::string get_role_description() const override {
        return "Dr. " + _name + " (" + _specialization + ") performs clinical evaluations and determines resource requisitions.";
    }

    // Method Overriding (Polymorphism)
    std::vector<std::string> get_permissions() const override {
        return {"view_patients", "conduct_assessment", "request_resources"};
    }

    // Domain validation method
    bool validate_assessment_input(const std::string& findings,
                                   const std::string& urgency_level,
                                   const std::map<int, int>& requested_resources,
                                   std::string& out_error) const {
        if (findings.empty() || findings.find_first_not_of(" \t\n\r") == std::string::npos) {
            out_error = "Examination findings cannot be empty.";
            return false;
        }

        static const std::set<std::string> valid_urgencies = {"Low", "Medium", "High", "Critical"};
        if (!valid_urgencies.count(urgency_level)) {
            out_error = "Invalid urgency level '" + urgency_level + "'. Must be Low, Medium, High, or Critical.";
            return false;
        }

        // Validate quantities are non-negative
        for (const auto& pair : requested_resources) {
            if (pair.second < 0) {
                out_error = "Requested quantity for resource ID " + std::to_string(pair.first) + " cannot be negative.";
                return false;
            }
        }

        // Note: Zero requested resources is allowed (assessment only, no resources needed)
        return true;
    }
};

// ============================================================================
// 4. Administrator Class (Demonstrates Inheritance & Method Overriding)
// ============================================================================
class Administrator : public HospitalStaff {
public:
    Administrator(int staff_id, const std::string& name,
                  const std::string& department = "Hospital Operations")
        : HospitalStaff(staff_id, name, department, "Administrator") {}

    // Method Overriding (Polymorphism)
    std::string get_role_description() const override {
        return "Administrator " + _name + " oversees resource capacity, releases allocations, and reallocates queues.";
    }

    // Method Overriding (Polymorphism)
    std::vector<std::string> get_permissions() const override {
        return {"view_inventory", "update_inventory", "release_resources", "process_waiting_queue"};
    }

    // Business rule validation: total inventory cannot be decreased below active allocated units
    bool validate_inventory_update(int current_allocated, int new_total, std::string& out_error) const {
        if (new_total < 0) {
            out_error = "Total quantity cannot be negative.";
            return false;
        }
        if (new_total < current_allocated) {
            out_error = "Cannot decrease total capacity (" + std::to_string(new_total) +
                        ") below currently allocated units (" + std::to_string(current_allocated) +
                        "). Release active allocations first.";
            return false;
        }
        return true;
    }
};

// ============================================================================
// 5. Resource Class (Demonstrates Encapsulation & Capacity Rules)
// ============================================================================
class Resource {
private:
    int _resource_id;
    std::string _resource_type;
    std::string _name;
    int _total_quantity;
    int _allocated_quantity;

public:
    Resource(int id, const std::string& type, const std::string& name, int total, int allocated = 0)
        : _resource_id(id), _resource_type(type), _name(name), _total_quantity(total), _allocated_quantity(allocated) {
        if (total < 0) throw std::invalid_argument("Total quantity cannot be negative.");
        if (allocated < 0) throw std::invalid_argument("Allocated quantity cannot be negative.");
        if (allocated > total) throw std::invalid_argument("Allocated quantity cannot exceed total quantity.");
    }

    // Encapsulated Getters
    int get_id() const { return _resource_id; }
    const std::string& get_resource_type() const { return _resource_type; }
    const std::string& get_name() const { return _name; }
    int get_total_quantity() const { return _total_quantity; }
    int get_allocated_quantity() const { return _allocated_quantity; }

    // Mathematical consistency: available = total - allocated
    int get_available_quantity() const {
        return _total_quantity - _allocated_quantity;
    }

    bool can_allocate(int quantity) const {
        if (quantity <= 0) return false;
        return get_available_quantity() >= quantity;
    }

    bool allocate(int quantity, std::string& out_error) {
        if (quantity <= 0) {
            out_error = "Allocation quantity must be greater than zero.";
            return false;
        }
        if (!can_allocate(quantity)) {
            out_error = "Insufficient capacity for '" + _name + "'. Requested: " +
                        std::to_string(quantity) + ", Available: " + std::to_string(get_available_quantity());
            return false;
        }
        _allocated_quantity += quantity;
        return true;
    }

    bool release(int quantity, std::string& out_error) {
        if (quantity <= 0) {
            out_error = "Release quantity must be greater than zero.";
            return false;
        }
        if (quantity > _allocated_quantity) {
            out_error = "Cannot release " + std::to_string(quantity) + " units of '" + _name +
                        "'. Only " + std::to_string(_allocated_quantity) + " currently allocated.";
            return false;
        }
        _allocated_quantity -= quantity;
        return true;
    }

    bool update_total_capacity(int new_total, std::string& out_error) {
        if (new_total < 0) {
            out_error = "Capacity cannot be negative.";
            return false;
        }
        if (new_total < _allocated_quantity) {
            out_error = "Cannot reduce capacity to " + std::to_string(new_total) + "; " +
                        std::to_string(_allocated_quantity) + " units are actively in use.";
            return false;
        }
        _total_quantity = new_total;
        return true;
    }

    json to_json() const {
        return json{
            {"id", _resource_id},
            {"resource_type", _resource_type},
            {"name", _name},
            {"total_quantity", _total_quantity},
            {"allocated_quantity", _allocated_quantity},
            {"available_quantity", get_available_quantity()}
        };
    }
};

// ============================================================================
// 6. Allocation Class (Demonstrates Lifecycle & Duplicate Release Prevention)
// ============================================================================
class Allocation {
private:
    int _allocation_id;
    int _patient_id;
    int _assessment_id;
    int _resource_id;
    int _allocated_quantity;
    std::string _status;
    std::string _allocated_at;
    std::string _released_at;

public:
    Allocation(int alloc_id, int pat_id, int assess_id, int res_id, int qty,
               const std::string& status = "Allocated",
               const std::string& alloc_at = "",
               const std::string& rel_at = "")
        : _allocation_id(alloc_id), _patient_id(pat_id), _assessment_id(assess_id),
          _resource_id(res_id), _allocated_quantity(qty), _status(status),
          _allocated_at(alloc_at), _released_at(rel_at) {
        if (qty <= 0) throw std::invalid_argument("Allocated quantity must be positive.");
    }

    int get_id() const { return _allocation_id; }
    int get_patient_id() const { return _patient_id; }
    int get_assessment_id() const { return _assessment_id; }
    int get_resource_id() const { return _resource_id; }
    int get_allocated_quantity() const { return _allocated_quantity; }
    const std::string& get_status() const { return _status; }

    bool is_active() const {
        return _status == "Allocated";
    }

    // Encapsulation: enforce status transition rule
    bool mark_released(const std::string& release_timestamp, std::string& out_error) {
        if (_status != "Allocated") {
            out_error = "Cannot release allocation #" + std::to_string(_allocation_id) +
                        ": current status is '" + _status + "'. Duplicate release prevented.";
            return false;
        }
        _status = "Released";
        _released_at = release_timestamp;
        return true;
    }

    json to_json() const {
        return json{
            {"id", _allocation_id},
            {"patient_id", _patient_id},
            {"assessment_id", _assessment_id},
            {"resource_id", _resource_id},
            {"allocated_quantity", _allocated_quantity},
            {"status", _status},
            {"allocated_at", _allocated_at.empty() ? nullptr : json(_allocated_at)},
            {"released_at", _released_at.empty() ? nullptr : json(_released_at)}
        };
    }
};

#endif // MODELS_H

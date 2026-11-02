-- Intelligent Healthcare Resource Allocation Simulator
-- Database Schema & Initial Data
-- MySQL 8.0+ Compatible

CREATE DATABASE IF NOT EXISTS healthcare_simulator;
USE healthcare_simulator;

-- 1. Patients Table
CREATE TABLE IF NOT EXISTS patients (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    age INT NOT NULL,
    gender VARCHAR(20) NOT NULL,
    contact VARCHAR(20) NOT NULL,
    reason_for_visit TEXT NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'Awaiting Assessment',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_patient_age CHECK (age > 0)
) ENGINE=InnoDB;

-- 2. Hospital Resources Table
-- Maintains total and allocated counts.
-- available_quantity is computed automatically as (total_quantity - allocated_quantity)
CREATE TABLE IF NOT EXISTS resources (
    id INT AUTO_INCREMENT PRIMARY KEY,
    resource_type VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(100) NOT NULL,
    total_quantity INT NOT NULL DEFAULT 0,
    allocated_quantity INT NOT NULL DEFAULT 0,
    available_quantity INT GENERATED ALWAYS AS (total_quantity - allocated_quantity) STORED,
    CONSTRAINT chk_res_total CHECK (total_quantity >= 0),
    CONSTRAINT chk_res_allocated CHECK (allocated_quantity >= 0),
    CONSTRAINT chk_res_availability CHECK (total_quantity >= allocated_quantity)
) ENGINE=InnoDB;

-- 3. Doctor Assessments Table
-- Urgency level is used strictly for queue prioritization in the educational simulator
CREATE TABLE IF NOT EXISTS doctor_assessments (
    id INT AUTO_INCREMENT PRIMARY KEY,
    patient_id INT NOT NULL,
    doctor_name VARCHAR(100) NOT NULL,
    examination_findings TEXT NOT NULL,
    urgency_level ENUM('Low', 'Medium', 'High', 'Critical') NOT NULL,
    assessment_status VARCHAR(50) NOT NULL DEFAULT 'Pending',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_assessment_patient FOREIGN KEY (patient_id) 
        REFERENCES patients(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 4. Assessment Resource Requirements Table
-- Stores resources demanded per doctor assessment
CREATE TABLE IF NOT EXISTS assessment_resource_requirements (
    id INT AUTO_INCREMENT PRIMARY KEY,
    assessment_id INT NOT NULL,
    resource_id INT NOT NULL,
    requested_quantity INT NOT NULL,
    CONSTRAINT chk_req_qty CHECK (requested_quantity > 0),
    CONSTRAINT fk_req_assessment FOREIGN KEY (assessment_id) 
        REFERENCES doctor_assessments(id) ON DELETE CASCADE,
    CONSTRAINT fk_req_resource FOREIGN KEY (resource_id) 
        REFERENCES resources(id) ON DELETE RESTRICT
) ENGINE=InnoDB;

-- 5. Resource Allocations Table
-- Records active and released allocations with audit timestamps
CREATE TABLE IF NOT EXISTS resource_allocations (
    id INT AUTO_INCREMENT PRIMARY KEY,
    patient_id INT NOT NULL,
    assessment_id INT NOT NULL,
    resource_id INT NOT NULL,
    allocated_quantity INT NOT NULL,
    status ENUM('Allocated', 'Released', 'Cancelled') NOT NULL DEFAULT 'Allocated',
    allocated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    released_at TIMESTAMP NULL DEFAULT NULL,
    CONSTRAINT chk_alloc_qty CHECK (allocated_quantity > 0),
    CONSTRAINT fk_alloc_patient FOREIGN KEY (patient_id) 
        REFERENCES patients(id) ON DELETE CASCADE,
    CONSTRAINT fk_alloc_assessment FOREIGN KEY (assessment_id) 
        REFERENCES doctor_assessments(id) ON DELETE CASCADE,
    CONSTRAINT fk_alloc_resource FOREIGN KEY (resource_id) 
        REFERENCES resources(id) ON DELETE RESTRICT
) ENGINE=InnoDB;

-- Seed Initial Hospital Resources
INSERT INTO resources (resource_type, name, total_quantity, allocated_quantity)
VALUES 
    ('general_bed', 'General Bed', 20, 0),
    ('icu_bed', 'ICU Bed', 5, 0),
    ('ventilator', 'Ventilator', 4, 0),
    ('wheelchair', 'Wheelchair', 10, 0),
    ('doctor_slot', 'Doctor Availability Slot', 8, 0)
ON DUPLICATE KEY UPDATE 
    name = VALUES(name);

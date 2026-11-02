# Intelligent Healthcare Resource Allocation Simulator

## Project Overview

The **Intelligent Healthcare Resource Allocation Simulator** is a C++17-based project designed to simplify healthcare resource management using Object-Oriented Programming (OOP) principles.

The system manages patient information, hospital staff, patient assessments, resource availability, and resource allocation. It helps maintain proper allocation records and prevents invalid resource operations through data validation and exception handling.

## Objectives

- To manage patient information and hospital staff records.
- To assess patient requirements and urgency levels.
- To maintain hospital resource availability.
- To allocate resources efficiently based on patient requirements.
- To track resource allocation and release status.
- To demonstrate core OOP concepts using C++17.
- To ensure data validation and handle errors effectively.

## Key Features

- **Patient Management:** Maintains patient details such as ID, name, age, gender, and contact information.
- **Staff Management:** Represents doctors and administrators with different roles and permissions.
- **Patient Assessment:** Supports patient findings and urgency levels such as Low, Medium, High, and Critical.
- **Resource Management:** Tracks total, allocated, and available resources.
- **Resource Allocation:** Checks resource availability before allowing allocation.
- **Allocation Tracking:** Maintains allocation records and updates their status upon release.
- **Data Validation:** Prevents invalid inputs and incorrect resource operations.
- **JSON Support:** Converts object information into JSON format.

## OOP Concepts Implemented

| OOP Concept        | Implementation                                                                      |
| ------------------ | ----------------------------------------------------------------------------------- |
| Encapsulation      | Protecting patient and resource data using private data members and public methods. |
| Inheritance        | Doctor and Administrator inherit from HospitalStaff.                                |
| Abstraction        | HospitalStaff is implemented as an abstract base class.                             |
| Polymorphism       | Virtual functions and function overriding provide different staff behaviours.       |
| Exception Handling | Handles invalid inputs and resource allocation errors.                              |

## Technologies Used

- **Programming Language:** C++
- **Language Standard:** C++17
- **Frontend:** HTML, CSS, JavaScript
- **Backend:** C++
- **Database:** SQL
- **JSON Library:** nlohmann/json
- **Development Environment:** Visual Studio Code / Antigravity IDE
- **Version Control:** Git and GitHub

## Project Structure

```text
Intelligent-Healthcare-Resource-Allocation/
│
├── backend/
│   ├── db.cpp
│   ├── db.h
│   ├── http_lib.h
│   ├── json.hpp
│   ├── main.cpp
│   └── models.h
│
├── database/
│   └── schema.sql
│
├── frontend/
│   ├── index.html
│   ├── styles.css
│   └── app.js
│
├── .gitignore
└── README.md
```

## System Modules

1. **Patient Module:** Manages patient information and validation.
2. **Hospital Staff Module:** Defines common staff information and responsibilities.
3. **Doctor Module:** Handles patient assessment and urgency levels.
4. **Administrator Module:** Manages resource inventory and capacity updates.
5. **Resource Module:** Maintains resource quantities and availability.
6. **Allocation Module:** Records resource allocation and release details.

## Testing

The system is tested using different valid and invalid input scenarios to verify:

- Patient information validation.
- Staff role functionality.
- Resource availability calculations.
- Successful and invalid resource allocation.
- Resource capacity updates.
- Allocation release and duplicate-release prevention.
- JSON conversion and exception handling.

## Future Scope

- Integration with a real-time hospital database.
- User authentication and role-based access.
- Advanced patient prioritization.
- Real-time resource availability monitoring.
- Detailed reports and analytics.
- Integration with hospital management platforms.

## Conclusion

The Intelligent Healthcare Resource Allocation Simulator demonstrates how Object-Oriented Programming can be applied to healthcare resource management. By implementing encapsulation, inheritance, abstraction, polymorphism, and exception handling, the project provides a modular and maintainable approach to managing patients, staff, and healthcare resources.

## License

This project is developed for academic and educational purposes.

/**
 * Intelligent Healthcare Resource Allocation Simulator
 * Frontend Controller & API Integration (Vanilla JavaScript)
 */

// Determine backend API host dynamically
const API_BASE = "http://127.0.0.1:5000";

// Global in-memory state
let currentResources = [];
let selectedDoctorPatientId = null;

// Initialize on DOM Ready
document.addEventListener("DOMContentLoaded", () => {
    initTabs();
    initForms();
    initModals();
    checkSystemHealth();

    // Initial data fetch
    loadReceptionistData();
    loadDoctorData();
    loadAdminData();

    // Periodic health refresh every 30 seconds
    setInterval(checkSystemHealth, 30000);
});

/* ==========================================================================
   Toast Notification System
   ========================================================================== */
function showToast(message, type = "info") {
    const container = document.getElementById("toast-container");
    if (!container) return;

    const toast = document.createElement("div");
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <span>${escapeHtml(message)}</span>
        <button style="background:none;border:none;color:#fff;cursor:pointer;font-weight:bold;">&times;</button>
    `;

    toast.querySelector("button").addEventListener("click", () => toast.remove());

    container.appendChild(toast);
    setTimeout(() => {
        if (toast.parentElement) toast.remove();
    }, 4500);
}

function escapeHtml(str) {
    if (typeof str !== "string") return str;
    return str.replace(/[&<>"']/g, (m) => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    }[m]));
}

/* ==========================================================================
   Navigation Tabs
   ========================================================================== */
function initTabs() {
    const tabs = document.querySelectorAll(".tab-btn");
    tabs.forEach(tab => {
        tab.addEventListener("click", () => {
            const targetView = tab.getAttribute("data-tab");

            // Update tab button styles
            tabs.forEach(t => t.classList.remove("active"));
            tab.classList.add("active");

            // Switch view
            document.querySelectorAll(".dashboard-view").forEach(v => v.classList.remove("active"));
            const activeView = document.getElementById(targetView);
            if (activeView) activeView.classList.add("active");

            // Refresh corresponding view data
            if (targetView === "view-receptionist") loadReceptionistData();
            if (targetView === "view-doctor") loadDoctorData();
            if (targetView === "view-admin") loadAdminData();
        });
    });
}

/* ==========================================================================
   System Health Check
   ========================================================================== */
async function checkSystemHealth() {
    const statusDot = document.getElementById("status-dot");
    const statusText = document.getElementById("status-text");

    try {
        const res = await fetch(`${API_BASE}/api/health`);
        const data = await res.json();

        if (res.ok && data.database && data.database.connected) {
            statusDot.className = "status-dot online";
            statusText.innerText = "System Online (MySQL Connected)";
        } else {
            statusDot.className = "status-dot offline";
            statusText.innerText = data.database?.error ? `DB Error: ${data.database.error}` : "Database Disconnected";
        }
    } catch (err) {
        statusDot.className = "status-dot offline";
        statusText.innerText = "Backend Offline (Check Flask server)";
    }
}

/* ==========================================================================
   Receptionist Dashboard
   ========================================================================== */
async function loadReceptionistData() {
    const tableBody = document.getElementById("patients-table-body");
    const countBadge = document.getElementById("receptionist-count");
    if (!tableBody) return;

    try {
        const res = await fetch(`${API_BASE}/api/patients`);
        const data = await res.json();

        if (!data.success) {
            showToast(data.error || "Failed to load patient records", "error");
            return;
        }

        const patients = data.patients || [];
        if (countBadge) countBadge.innerText = patients.length;

        if (patients.length === 0) {
            tableBody.innerHTML = `
                <tr>
                    <td colspan="6" class="empty-state">
                        <div class="empty-icon">📋</div>
                        <div>No patients registered yet.</div>
                    </td>
                </tr>`;
            return;
        }

        tableBody.innerHTML = patients.map(p => `
            <tr>
                <td><strong>#${p.id}</strong></td>
                <td>
                    <div style="font-weight:600;">${escapeHtml(p.name)}</div>
                    <div style="font-size:0.78rem;color:var(--text-muted);">${p.age} yrs &bull; ${escapeHtml(p.gender)}</div>
                </td>
                <td>${escapeHtml(p.contact)}</td>
                <td>${escapeHtml(p.reason_for_visit)}</td>
                <td>${renderStatusBadge(p.status)}</td>
                <td>
                    <button class="btn btn-secondary btn-sm" onclick="viewPatientDetails(${p.id})">
                        View Details
                    </button>
                </td>
            </tr>
        `).join("");

    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="6" style="color:red;text-align:center;">Failed to connect to backend server.</td></tr>`;
    }
}

function renderStatusBadge(status) {
    let cls = "badge-awaiting";
    if (status === "Resources Allocated") cls = "badge-allocated";
    else if (status === "Waiting for Resources") cls = "badge-waiting";
    else if (status === "Discharged") cls = "badge-discharged";

    return `<span class="badge ${cls}">${escapeHtml(status)}</span>`;
}

function initForms() {
    // Receptionist Patient Registration
    const regForm = document.getElementById("patient-reg-form");
    if (regForm) {
        regForm.addEventListener("submit", async (e) => {
            e.preventDefault();
            const submitBtn = regForm.querySelector('button[type="submit"]');
            submitBtn.disabled = true;
            submitBtn.innerText = "Registering...";

            const payload = {
                name: document.getElementById("reg-name").value.trim(),
                age: parseInt(document.getElementById("reg-age").value, 10),
                gender: document.getElementById("reg-gender").value,
                contact: document.getElementById("reg-contact").value.trim(),
                reason_for_visit: document.getElementById("reg-reason").value.trim()
            };

            try {
                const res = await fetch(`${API_BASE}/api/patients`, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify(payload)
                });
                const result = await res.json();

                if (result.success) {
                    showToast(result.message, "success");
                    regForm.reset();
                    loadReceptionistData();
                    loadDoctorData(); // Refresh doctor queue as well
                } else {
                    showToast(result.error || "Failed to register patient", "error");
                }
            } catch (err) {
                showToast("Connection failed. Ensure Flask backend is running.", "error");
            } finally {
                submitBtn.disabled = false;
                submitBtn.innerText = "Register Patient";
            }
        });
    }

    // Doctor Assessment Submission
    const assessForm = document.getElementById("doctor-assessment-form");
    if (assessForm) {
        assessForm.addEventListener("submit", async (e) => {
            e.preventDefault();

            if (!selectedDoctorPatientId) {
                showToast("Please select a patient from the awaiting list first.", "warning");
                return;
            }

            const findings = document.getElementById("doc-findings").value.trim();
            const urgency = document.getElementById("doc-urgency").value;
            const doctorName = document.getElementById("doc-name").value.trim() || "Dr. Sarah Jenkins";

            // Gather resource quantities
            const requested_resources = {};
            currentResources.forEach(r => {
                const input = document.getElementById(`res-req-${r.id}`);
                const qty = input ? parseInt(input.value, 10) : 0;
                if (qty > 0) {
                    requested_resources[r.id] = qty;
                }
            });

            // If zero resources are specified, the assessment is saved without resource allocation

            const submitBtn = assessForm.querySelector('button[type="submit"]');
            submitBtn.disabled = true;
            submitBtn.innerText = "Evaluating & Allocating...";

            try {
                const res = await fetch(`${API_BASE}/api/assessments`, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({
                        patient_id: selectedDoctorPatientId,
                        doctor_name: doctorName,
                        examination_findings: findings,
                        urgency_level: urgency,
                        requested_resources: requested_resources
                    })
                });

                const result = await res.json();
                const banner = document.getElementById("doctor-feedback-banner");

                if (result.success) {
                    if (result.allocated) {
                        banner.className = "feedback-banner success";
                        banner.innerHTML = `<strong>Allocation Success!</strong> ${escapeHtml(result.message)}`;
                        showToast("All requested resources successfully allocated!", "success");
                    } else if (result.shortages && result.shortages.length > 0) {
                        banner.className = "feedback-banner warning";
                        banner.innerHTML = `<strong>Resource Shortage Detected:</strong> ${escapeHtml(result.message)}<br><small>Patient added to priority Waiting Queue.</small>`;
                        showToast("Shortage detected: Patient placed on Waiting Queue.", "warning");
                    } else {
                        banner.className = "feedback-banner success";
                        banner.innerHTML = `<strong>Assessment Recorded:</strong> ${escapeHtml(result.message)}`;
                        showToast(result.message, "success");
                    }

                    // Reset form and refresh all relevant panels
                    assessForm.reset();
                    selectedDoctorPatientId = null;
                    document.getElementById("selected-patient-header").innerText = "No Patient Selected";
                    document.getElementById("selected-patient-sub").innerText = "Select an awaiting patient on the left";

                    loadDoctorData();
                    loadReceptionistData();
                    loadAdminData();
                } else {
                    showToast(result.error || "Assessment submission failed", "error");
                }
            } catch (err) {
                showToast("Network error communicating with Flask backend.", "error");
            } finally {
                submitBtn.disabled = false;
                submitBtn.innerText = "Submit Assessment & Allocate";
            }
        });
    }

    // Admin Capacity Update Form
    const capForm = document.getElementById("update-capacity-form");
    if (capForm) {
        capForm.addEventListener("submit", async (e) => {
            e.preventDefault();
            const resId = document.getElementById("cap-resource-id").value;
            const newTotal = document.getElementById("cap-total-qty").value;

            try {
                const res = await fetch(`${API_BASE}/api/resources/update`, {
                    method: "POST",
                    headers: { "Content-Type": "application/json" },
                    body: JSON.stringify({
                        resource_id: parseInt(resId, 10),
                        total_quantity: parseInt(newTotal, 10)
                    })
                });
                const result = await res.json();

                if (result.success) {
                    showToast(result.message, "success");
                    closeModal("capacity-modal");
                    loadAdminData();
                    loadDoctorData(); // Refresh picker availabilities
                } else {
                    showToast(result.error || "Failed to update inventory", "error");
                }
            } catch (err) {
                showToast("Error updating capacity", "error");
            }
        });
    }
}

/* ==========================================================================
   Doctor Dashboard
   ========================================================================== */
async function loadDoctorData() {
    await fetchResources();
    await loadAwaitingPatients();
    renderDoctorResourcePickers();
}

async function loadAwaitingPatients() {
    const listContainer = document.getElementById("awaiting-patients-list");
    const countBadge = document.getElementById("doctor-awaiting-count");
    if (!listContainer) return;

    try {
        const res = await fetch(`${API_BASE}/api/patients/awaiting-assessment`);
        const data = await res.json();

        if (!data.success) {
            listContainer.innerHTML = `<div style="color:red;padding:1rem;">Failed to load awaiting patients</div>`;
            return;
        }

        const patients = data.patients || [];
        if (countBadge) countBadge.innerText = patients.length;

        if (patients.length === 0) {
            listContainer.innerHTML = `
                <div class="empty-state">
                    <div class="empty-icon">✅</div>
                    <div>No patients currently awaiting assessment.</div>
                </div>`;
            return;
        }

        listContainer.innerHTML = patients.map(p => `
            <div class="patient-select-item ${selectedDoctorPatientId === p.id ? 'selected' : ''}" onclick="selectPatientForDoctor(${p.id}, '${escapeHtml(p.name)}', ${p.age}, '${escapeHtml(p.gender)}', '${escapeHtml(p.reason_for_visit)}')">
                <div class="item-top">
                    <span>#${p.id} &bull; ${escapeHtml(p.name)}</span>
                    <span style="font-size:0.75rem;color:var(--text-muted);">${p.age}y / ${escapeHtml(p.gender)}</span>
                </div>
                <div class="item-desc"><strong>Reason:</strong> ${escapeHtml(p.reason_for_visit)}</div>
            </div>
        `).join("");

    } catch (err) {
        listContainer.innerHTML = `<div style="color:red;padding:1rem;">Backend server unreachable</div>`;
    }
}

window.selectPatientForDoctor = function(id, name, age, gender, reason) {
    selectedDoctorPatientId = id;

    // Highlight selected item
    document.querySelectorAll(".patient-select-item").forEach(item => {
        item.classList.remove("selected");
    });
    if (event && event.currentTarget) {
        event.currentTarget.classList.add("selected");
    }

    // Update Header
    document.getElementById("selected-patient-header").innerText = `Patient: #${id} — ${name}`;
    document.getElementById("selected-patient-sub").innerText = `${age} yrs &bull; ${gender} &bull; Reason: ${reason}`;

    // Reset feedback banner
    const banner = document.getElementById("doctor-feedback-banner");
    if (banner) banner.style.display = "none";
};

async function fetchResources() {
    try {
        const res = await fetch(`${API_BASE}/api/resources`);
        const data = await res.json();
        if (data.success) {
            currentResources = data.resources || [];
        }
    } catch (err) {
        console.error("Failed to fetch resources:", err);
    }
}

function renderDoctorResourcePickers() {
    const container = document.getElementById("doctor-resource-pickers");
    if (!container) return;

    if (currentResources.length === 0) {
        container.innerHTML = `<div>No resources found in database inventory.</div>`;
        return;
    }

    container.innerHTML = currentResources.map(r => `
        <div class="resource-picker-card">
            <div class="picker-label">
                <span>${escapeHtml(r.name)}</span>
                <span class="picker-avail">${r.available_quantity} Avail</span>
            </div>
            <input type="number" id="res-req-${r.id}" min="0" max="${r.total_quantity}" value="0" placeholder="Qty">
        </div>
    `).join("");
}

/* ==========================================================================
   Administrator Dashboard
   ========================================================================== */
async function loadAdminData() {
    await fetchResources();
    renderAdminInventory();
    await loadAdminWaitingQueue();
    await loadAdminAllocations();
}

function renderAdminInventory() {
    const grid = document.getElementById("admin-inventory-grid");
    if (!grid) return;

    if (currentResources.length === 0) {
        grid.innerHTML = `<div class="empty-state">No inventory configured. Initialize database first.</div>`;
        return;
    }

    grid.innerHTML = currentResources.map(r => {
        const percent = r.total_quantity > 0 ? Math.round((r.allocated_quantity / r.total_quantity) * 100) : 0;
        let barClass = "";
        if (percent >= 90) barClass = "full";
        else if (percent >= 70) barClass = "high";

        return `
            <div class="resource-card">
                <div>
                    <div class="res-header">
                        <div class="res-title">${escapeHtml(r.name)}</div>
                        <div class="res-icon">📦</div>
                    </div>
                    <div class="res-stats">
                        <div class="stat-item">
                            <div class="stat-val">${r.total_quantity}</div>
                            <div class="stat-lbl">Total</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-val" style="color:var(--status-high);">${r.allocated_quantity}</div>
                            <div class="stat-lbl">In Use</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-val available">${r.available_quantity}</div>
                            <div class="stat-lbl">Available</div>
                        </div>
                    </div>
                    <div class="progress-container">
                        <div class="progress-bar ${barClass}" style="width: ${percent}%;"></div>
                    </div>
                    <div style="font-size:0.75rem;color:var(--text-muted);margin-bottom:1rem;text-align:right;">
                        ${percent}% capacity utilized
                    </div>
                </div>
                <button class="btn btn-secondary btn-sm btn-block" onclick="openUpdateCapacityModal(${r.id}, '${escapeHtml(r.name)}', ${r.total_quantity}, ${r.allocated_quantity})">
                    ⚙️ Adjust Capacity
                </button>
            </div>
        `;
    }).join("");
}

async function loadAdminWaitingQueue() {
    const tableBody = document.getElementById("admin-waiting-table-body");
    const countBadge = document.getElementById("admin-waiting-count");
    if (!tableBody) return;

    try {
        const res = await fetch(`${API_BASE}/api/waiting-list`);
        const data = await res.json();

        if (!data.success) {
            tableBody.innerHTML = `<tr><td colspan="6" style="color:red;text-align:center;">Failed to load waiting queue</td></tr>`;
            return;
        }

        const queue = data.waiting_queue || [];
        if (countBadge) countBadge.innerText = queue.length;

        if (queue.length === 0) {
            tableBody.innerHTML = `
                <tr>
                    <td colspan="6" class="empty-state">
                        <div class="empty-icon">🎉</div>
                        <div>No patients waiting. All resource requests have been fulfilled!</div>
                    </td>
                </tr>`;
            return;
        }

        tableBody.innerHTML = queue.map((item, idx) => {
            const reqList = (item.requirements || []).map(r => `
                <span class="badge ${r.available_quantity < r.requested_quantity ? 'badge-waiting' : 'badge-allocated'}">
                    ${escapeHtml(r.resource_name)}: ${r.requested_quantity} (Avail: ${r.available_quantity})
                </span>
            `).join(" ");

            let urgencyCls = "badge-awaiting";
            if (item.urgency_level === "Critical") urgencyCls = "badge-critical";
            else if (item.urgency_level === "High") urgencyCls = "badge-waiting";

            return `
                <tr>
                    <td><strong>#${idx + 1}</strong></td>
                    <td><span class="badge ${urgencyCls}">${escapeHtml(item.urgency_level)}</span></td>
                    <td>
                        <strong>${escapeHtml(item.patient_name)}</strong> (ID: #${item.patient_id})<br>
                        <small style="color:var(--text-muted);">${item.age}y / ${escapeHtml(item.gender)}</small>
                    </td>
                    <td>${reqList || 'None specified'}</td>
                    <td style="font-size:0.8rem;color:var(--text-muted);">${item.assessment_time || 'N/A'}</td>
                    <td>
                        <button class="btn btn-secondary btn-sm" onclick="viewPatientDetails(${item.patient_id})">
                            Details
                        </button>
                    </td>
                </tr>
            `;
        }).join("");

    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="6" style="color:red;text-align:center;">Network error loading waiting queue</td></tr>`;
    }
}

// Process waiting queue action
window.processWaitingQueue = async function() {
    const btn = document.getElementById("btn-process-waiting");
    if (btn) {
        btn.disabled = true;
        btn.innerText = "Processing...";
    }

    try {
        const res = await fetch(`${API_BASE}/api/waiting-list/process`, {
            method: "POST"
        });
        const data = await res.json();

        if (data.success) {
            showToast(data.message, data.allocated_count > 0 ? "success" : "info");
            loadAdminData();
            loadReceptionistData();
            loadDoctorData();
        } else {
            showToast(data.error || "Failed to process queue", "error");
        }
    } catch (err) {
        showToast("Error processing waiting queue", "error");
    } finally {
        if (btn) {
            btn.disabled = false;
            btn.innerText = "⚡ Process Waiting Requests";
        }
    }
};

async function loadAdminAllocations() {
    const tableBody = document.getElementById("admin-allocations-table-body");
    if (!tableBody) return;

    try {
        const res = await fetch(`${API_BASE}/api/allocations`);
        const data = await res.json();

        if (!data.success) {
            tableBody.innerHTML = `<tr><td colspan="7" style="color:red;text-align:center;">Failed to load allocations</td></tr>`;
            return;
        }

        const allocations = data.allocations || [];
        if (allocations.length === 0) {
            tableBody.innerHTML = `
                <tr>
                    <td colspan="7" class="empty-state">
                        <div class="empty-icon">📂</div>
                        <div>No allocation records recorded yet.</div>
                    </td>
                </tr>`;
            return;
        }

        tableBody.innerHTML = allocations.map(a => `
            <tr>
                <td><strong>#${a.allocation_id}</strong></td>
                <td><strong>${escapeHtml(a.patient_name)}</strong> (ID: #${a.patient_id})</td>
                <td>${escapeHtml(a.resource_name)}</td>
                <td><strong>${a.allocated_quantity}</strong> units</td>
                <td>
                    <span class="badge ${a.status === 'Allocated' ? 'badge-allocated' : 'badge-discharged'}">
                        ${a.status}
                    </span>
                </td>
                <td style="font-size:0.8rem;color:var(--text-muted);">${a.allocated_at || 'N/A'}</td>
                <td>
                    ${a.status === 'Allocated' ? `
                        <button class="btn btn-danger btn-sm" onclick="releaseAllocation(${a.allocation_id})">
                            Release Resource
                        </button>
                    ` : `<span style="font-size:0.75rem;color:var(--text-muted);">Released at ${a.released_at || ''}</span>`}
                </td>
            </tr>
        `).join("");

    } catch (err) {
        tableBody.innerHTML = `<tr><td colspan="7" style="color:red;text-align:center;">Failed to retrieve allocations</td></tr>`;
    }
}

window.releaseAllocation = async function(allocationId) {
    if (!confirm(`Are you sure you want to release allocation record #${allocationId} and return the resource to inventory?`)) {
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/allocations/${allocationId}/release`, {
            method: "POST"
        });
        const data = await res.json();

        if (data.success) {
            showToast(data.message, "success");
            loadAdminData();
            loadReceptionistData();
            loadDoctorData();
        } else {
            showToast(data.error || "Failed to release resource", "error");
        }
    } catch (err) {
        showToast("Error releasing allocation record", "error");
    }
};

/* ==========================================================================
   Modals & Details
   ========================================================================== */
function initModals() {
    // Close modal when clicking overlay background
    document.querySelectorAll(".modal-overlay").forEach(overlay => {
        overlay.addEventListener("click", (e) => {
            if (e.target === overlay) {
                overlay.classList.remove("active");
            }
        });
    });
}

window.closeModal = function(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) modal.classList.remove("active");
};

window.openUpdateCapacityModal = function(id, name, total, allocated) {
    document.getElementById("cap-resource-id").value = id;
    document.getElementById("cap-resource-name").innerText = name;
    document.getElementById("cap-allocated-info").innerText = allocated;
    document.getElementById("cap-total-qty").value = total;
    document.getElementById("cap-total-qty").min = allocated;

    document.getElementById("capacity-modal").classList.add("active");
};

window.viewPatientDetails = async function(patientId) {
    const modal = document.getElementById("patient-details-modal");
    const content = document.getElementById("patient-details-content");
    if (!modal || !content) return;

    modal.classList.add("active");
    content.innerHTML = `<div style="text-align:center;padding:2rem;">Loading patient history...</div>`;

    try {
        const res = await fetch(`${API_BASE}/api/patients/${patientId}`);
        const data = await res.json();

        if (!data.success) {
            content.innerHTML = `<div style="color:red;">${data.error || "Failed to load details"}</div>`;
            return;
        }

        const p = data.patient;
        const assessments = data.assessments || [];
        const allocations = data.allocations || [];

        const activeAllocationsCount = allocations.filter(a => a.status === 'Allocated').length;

        content.innerHTML = `
            <div style="border-bottom:1px solid var(--border);padding-bottom:1rem;margin-bottom:1rem;">
                <div style="display:flex;justify-content:space-between;align-items:center;">
                    <h3 style="font-size:1.15rem;font-weight:700;">#${p.id} — ${escapeHtml(p.name)}</h3>
                    ${renderStatusBadge(p.status)}
                </div>
                <div style="font-size:0.85rem;color:var(--text-muted);margin-top:0.35rem;">
                    <strong>Age/Gender:</strong> ${p.age} yrs, ${escapeHtml(p.gender)} &bull; 
                    <strong>Contact:</strong> ${escapeHtml(p.contact)} &bull;
                    <strong>Registered:</strong> ${p.created_at || 'N/A'}
                </div>
                <div style="font-size:0.85rem;margin-top:0.4rem;">
                    <strong>Reason for Visit:</strong> ${escapeHtml(p.reason_for_visit)}
                </div>
            </div>

            <h4 style="font-size:0.95rem;margin-bottom:0.5rem;">🩺 Doctor Assessments (${assessments.length})</h4>
            ${assessments.length === 0 ? '<p style="font-size:0.85rem;color:var(--text-muted);margin-bottom:1rem;">No clinical assessment recorded yet.</p>' : `
                <div style="display:flex;flex-direction:column;gap:0.75rem;margin-bottom:1.25rem;">
                    ${assessments.map(a => `
                        <div style="background:#f8fafc;padding:0.75rem;border-radius:var(--radius-sm);border:1px solid var(--border);font-size:0.85rem;">
                            <div style="display:flex;justify-content:space-between;margin-bottom:0.3rem;">
                                <strong>${escapeHtml(a.doctor_name)}</strong>
                                <span class="badge ${a.urgency_level === 'Critical' ? 'badge-critical' : 'badge-awaiting'}">
                                    Urgency: ${a.urgency_level}
                                </span>
                            </div>
                            <div><strong>Findings:</strong> ${escapeHtml(a.examination_findings)}</div>
                            <div style="margin-top:0.3rem;"><strong>Resource Requisitions:</strong>
                                ${(a.requirements || []).map(r => `${r.resource_name} (${r.requested_quantity})`).join(", ") || 'None'}
                            </div>
                        </div>
                    `).join("")}
                </div>
            `}

            <h4 style="font-size:0.95rem;margin-bottom:0.5rem;">📦 Resource Allocation Records (${allocations.length})</h4>
            ${allocations.length === 0 ? '<p style="font-size:0.85rem;color:var(--text-muted);">No resources have been allocated.</p>' : `
                <div style="display:flex;flex-direction:column;gap:0.5rem;margin-bottom:1rem;">
                    ${allocations.map(al => `
                        <div style="display:flex;justify-content:space-between;align-items:center;background:#f8fafc;padding:0.6rem 0.8rem;border-radius:var(--radius-sm);border:1px solid var(--border);font-size:0.85rem;">
                            <div>
                                <strong>${escapeHtml(al.resource_name)}</strong>: ${al.allocated_quantity} units
                                <div style="font-size:0.75rem;color:var(--text-muted);">Allocated: ${al.allocated_at || 'N/A'}</div>
                            </div>
                            <span class="badge ${al.status === 'Allocated' ? 'badge-allocated' : 'badge-discharged'}">${al.status}</span>
                        </div>
                    `).join("")}
                </div>
            `}

            ${activeAllocationsCount > 0 ? `
                <div style="margin-top:1.25rem;border-top:1px solid var(--border);padding-top:1rem;text-align:right;">
                    <button class="btn btn-danger btn-sm" onclick="releaseAllForPatient(${p.id})">
                        Release All Active Resources & Discharge
                    </button>
                </div>
            ` : ''}
        `;

    } catch (err) {
        content.innerHTML = `<div style="color:red;">Error loading patient details.</div>`;
    }
};

window.releaseAllForPatient = async function(patientId) {
    if (!confirm(`Release all active resource allocations for patient #${patientId} and discharge the patient?`)) {
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/patients/${patientId}/release-all`, {
            method: "POST"
        });
        const data = await res.json();

        if (data.success) {
            showToast(data.message, "success");
            closeModal("patient-details-modal");
            loadAdminData();
            loadReceptionistData();
            loadDoctorData();
        } else {
            showToast(data.error || "Failed to release resources", "error");
        }
    } catch (err) {
        showToast("Error releasing patient resources", "error");
    }
};

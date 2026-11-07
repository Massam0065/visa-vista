// ===== Visa Vista Admin - FINAL =====
const API = "https://visa-vista-api-he8x.onrender.com";

let token = localStorage.getItem("vv_token") || "";

// Agar pehle se login hai to dashboard dikhao
function showDashboard(show) {
  const loginBox = document.getElementById("loginBox") || document.getElementById("login-box") || document.getElementById("loginSection");
  const dashBox = document.getElementById("dashboard") || document.getElementById("dash") || document.getElementById("dashboardSection");
  if (loginBox) loginBox.style.display = show ? "none" : "block";
  if (dashBox) dashBox.style.display = show ? "block" : "none";
  if (show) loadStories();
}

function showMsg(id, msg, isError) {
  const el = document.getElementById(id);
  if (!el) { if (msg) alert(msg); return; }
  el.style.display = msg ? "block" : "none";
  el.textContent = msg || "";
  el.style.color = isError ? "red" : "green";
}

// ===== LOGIN =====
async function doLogin(e) {
  if (e) e.preventDefault();
  const passInput = document.getElementById("password") || document.getElementById("adminPass") || document.getElementById("pass");
  const password = passInput ? passInput.value.trim() : "";
  if (!password) { showMsg("loginError", "Password likho", true); return; }

  showMsg("loginError", "Login ho raha hai... wait karo", false);

  try {
    const res = await fetch(API + "/api/login", {
      method: "POST",
      // IMPORTANT: text/plain is liye taake browser OPTIONS na bheje aur Failed to fetch na aaye
      headers: { "Content-Type": "text/plain" },
      body: JSON.stringify({ password: password })
    });

    const data = await res.json().catch(() => ({}));

    if (!res.ok) {
      showMsg("loginError", "Login failed: " + (data.error || "wrong password"), true);
      return;
    }

    token = data.token;
    localStorage.setItem("vv_token", token);
    showMsg("loginError", "", false);
    showDashboard(true);
  } catch (err) {
    showMsg("loginError", "Login failed: Failed to fetch | API=" + API, true);
  }
}

// ===== LOAD STORIES =====
async function loadStories() {
  const list = document.getElementById("storiesList") || document.getElementById("storyList") || document.getElementById("list");
  if (!list) return;
  list.innerHTML = "Loading...";
  try {
    const res = await fetch(API + "/api/stories");
    const data = await res.json();
    const stories = data.stories || [];
    if (stories.length === 0) {
      list.innerHTML = "<p>Abhi koi story nahi hai.</p>";
      return;
    }
    list.innerHTML = "";
    stories.forEach(s => {
      const div = document.createElement("div");
      div.style.border = "1px solid #ddd";
      div.style.padding = "10px";
      div.style.marginBottom = "10px";
      div.style.borderRadius = "8px";
      div.innerHTML = `
        <b>${s.title || "No Title"}</b> - ${s.country || ""} - ${s.category || ""}<br>
        <small>${s.date || ""} | ID: ${s.id}</small><br>
        ${s.image ? `<img src="${s.image}" style="width:100px;height:70px;object-fit:cover;margin-top:5px;border-radius:6px;">` : ""}<br>
        <button onclick="deleteStory('${s.id}')" style="margin-top:5px;background:red;color:white;border:none;padding:6px 12px;border-radius:6px;cursor:pointer;">Delete</button>
      `;
      list.appendChild(div);
    });
  } catch (e) {
    list.innerHTML = "Load nahi hua: " + e.message;
  }
}

// ===== ADD STORY =====
async function addStory(e) {
  if (e) e.preventDefault();
  const get = (id) => { const el = document.getElementById(id); return el ? el.value.trim() : ""; };
  const payload = {
    title: get("title"),
    category: get("category"),
    country: get("country"),
    date: get("date"),
    story: get("story"),
    image: get("image"),
    video: get("video")
  };
  if (!payload.title) { alert("Title likho"); return; }

  try {
    const res = await fetch(API + "/api/stories", {
      method: "POST",
      headers: { "Content-Type": "text/plain", "Authorization": "Bearer " + token },
      body: JSON.stringify(payload)
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      alert("Save nahi hua: " + (data.error || "Please log in again"));
      if (res.status === 401) { doLogout(); }
      return;
    }
    alert("Story save ho gayi!");
    const form = document.getElementById("storyForm") || document.getElementById("addForm");
    if (form) form.reset();
    loadStories();
  } catch (err) {
    alert("Save failed: Failed to fetch");
  }
}

// ===== DELETE =====
async function deleteStory(id) {
  if (!confirm("Is story ko delete karna hai?")) return;
  try {
    const res = await fetch(API + "/api/stories/" + encodeURIComponent(id), {
      method: "DELETE",
      headers: { "Authorization": "Bearer " + token }
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) { alert("Delete nahi hua: " + (data.error || "")); return; }
    alert("Delete ho gayi");
    loadStories();
  } catch (e) {
    alert("Delete failed: Failed to fetch");
  }
}

// ===== LOGOUT =====
function doLogout() {
  token = "";
  localStorage.removeItem("vv_token");
  showDashboard(false);
}

// ===== CONNECT BUTTONS =====
document.addEventListener("DOMContentLoaded", () => {
  const loginForm = document.getElementById("loginForm");
  if (loginForm) loginForm.addEventListener("submit", doLogin);
  const loginBtn = document.getElementById("loginBtn");
  if (loginBtn) loginBtn.addEventListener("click", doLogin);

  const storyForm = document.getElementById("storyForm") || document.getElementById("addForm");
  if (storyForm) storyForm.addEventListener("submit", addStory);
  const saveBtn = document.getElementById("saveBtn") || document.getElementById("publishBtn");
  if (saveBtn) saveBtn.addEventListener("click", addStory);

  const logoutBtn = document.getElementById("logoutBtn");
  if (logoutBtn) logoutBtn.addEventListener("click", doLogout);

  if (token) showDashboard(true);
  else showDashboard(false);
});

```javascript
// ===== VISA VISTA ADMIN - EDIT ENABLED =====

const API = "https://visa-vista-api-he8x.onrender.com";

let token = localStorage.getItem("vv_token") || "";
let editingId = null;
let allStories = [];

// ===== DASHBOARD =====

function showDashboard(show) {
  const loginBox =
    document.getElementById("loginBox") ||
    document.getElementById("login-box") ||
    document.getElementById("loginSection");

  const dashBox =
    document.getElementById("dashboard") ||
    document.getElementById("dash") ||
    document.getElementById("dashboardSection");

  if (loginBox) loginBox.style.display = show ? "none" : "block";
  if (dashBox) dashBox.style.display = show ? "block" : "none";

  if (show) loadStories();
}

function showMsg(id, msg, isError) {
  const el = document.getElementById(id);

  if (!el) {
    if (msg) alert(msg);
    return;
  }

  el.style.display = msg ? "block" : "none";
  el.textContent = msg || "";
  el.style.color = isError ? "red" : "green";
}

// ===== LOGIN =====

async function doLogin(e) {
  if (e) e.preventDefault();

  const passInput =
    document.getElementById("password") ||
    document.getElementById("adminPass") ||
    document.getElementById("pass");

  const password = passInput ? passInput.value.trim() : "";

  if (!password) {
    showMsg("loginError", "Password likho", true);
    return;
  }

  showMsg("loginError", "Login ho raha hai...", false);

  try {
    const res = await fetch(API + "/api/login", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: JSON.stringify({ password })
    });

    const data = await res.json().catch(() => ({}));

    if (!res.ok) {
      showMsg(
        "loginError",
        "Login failed: " + (data.error || "wrong password"),
        true
      );
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
  const list =
    document.getElementById("storiesList") ||
    document.getElementById("storyList") ||
    document.getElementById("list");

  if (!list) return;

  list.innerHTML = "Loading...";

  try {
    const res = await fetch(API + "/api/stories");
    const data = await res.json();

    if (!res.ok) throw new Error("Stories load nahi huin");

    allStories = data.stories || [];

    if (allStories.length === 0) {
      list.innerHTML = "<p>Abhi koi story nahi hai.</p>";
      return;
    }

    list.innerHTML = "";

    allStories.forEach(s => {
      const div = document.createElement("div");

      div.style.border = "1px solid #ddd";
      div.style.padding = "10px";
      div.style.marginBottom = "10px";
      div.style.borderRadius = "8px";

      const title = document.createElement("b");
      title.textContent = s.title || "No Title";

      const details = document.createElement("div");
      details.textContent =
        (s.country || "") + " - " +
        (s.category || "") + " - " +
        (s.date || "");

      const idText = document.createElement("small");
      idText.textContent = "ID: " + s.id;

      div.appendChild(title);
      div.appendChild(document.createElement("br"));
      div.appendChild(details);
      div.appendChild(document.createElement("br"));
      div.appendChild(idText);
      div.appendChild(document.createElement("br"));

      if (s.image) {
        const img = document.createElement("img");
        img.src = s.image;
        img.style.width = "100px";
        img.style.height = "70px";
        img.style.objectFit = "cover";
        img.style.marginTop = "5px";
        img.style.borderRadius = "6px";
        div.appendChild(img);
        div.appendChild(document.createElement("br"));
      }

      // EDIT BUTTON
      const editBtn = document.createElement("button");
      editBtn.textContent = "Edit";
      editBtn.type = "button";
      editBtn.style.marginTop = "5px";
      editBtn.style.background = "#087bca";
      editBtn.style.color = "white";
      editBtn.style.border = "none";
      editBtn.style.padding = "6px 12px";
      editBtn.style.borderRadius = "6px";
      editBtn.style.cursor = "pointer";

      editBtn.addEventListener("click", () => editStory(s.id));

      // DELETE BUTTON
      const deleteBtn = document.createElement("button");
      deleteBtn.textContent = "Delete";
      deleteBtn.type = "button";
      deleteBtn.style.marginTop = "5px";
      deleteBtn.style.marginLeft = "8px";
      deleteBtn.style.background = "red";
      deleteBtn.style.color = "white";
      deleteBtn.style.border = "none";
      deleteBtn.style.padding = "6px 12px";
      deleteBtn.style.borderRadius = "6px";
      deleteBtn.style.cursor = "pointer";

      deleteBtn.addEventListener("click", () => deleteStory(s.id));

      div.appendChild(editBtn);
      div.appendChild(deleteBtn);

      list.appendChild(div);
    });

  } catch (e) {
    list.innerHTML = "Load nahi hua: " + e.message;
  }
}

// ===== EDIT STORY =====

function editStory(id) {
  const story = allStories.find(s => String(s.id) === String(id));

  if (!story) {
    alert("Story nahi mili");
    return;
  }

  editingId = story.id;

  const fields = [
    "title",
    "category",
    "country",
    "date",
    "story"
  ];

  fields.forEach(field => {
    const el = document.getElementById(field);

    if (el) {
      el.value = story[field] || "";
    }
  });

  // Media fields blank rahenge.
  // Blank media ka matlab purani photo/video preserve karna hai.
  ["image", "video"].forEach(field => {
    const el = document.getElementById(field);
    if (el) el.value = "";
  });

  const saveBtn =
    document.getElementById("saveBtn") ||
    document.getElementById("publishBtn") ||
    document.querySelector("#storyForm button[type='submit']") ||
    document.querySelector("#addForm button[type='submit']");

  if (saveBtn) {
    saveBtn.textContent = "Update Story";
  }

  const form =
    document.getElementById("storyForm") ||
    document.getElementById("addForm");

  if (form) {
    let cancelBtn = document.getElementById("cancelEditBtn");

    if (!cancelBtn) {
      cancelBtn = document.createElement("button");
      cancelBtn.id = "cancelEditBtn";
      cancelBtn.type = "button";
      cancelBtn.textContent = "Cancel Edit";
      cancelBtn.style.marginLeft = "8px";
      cancelBtn.style.padding = "8px 14px";
      cancelBtn.style.cursor = "pointer";

      if (saveBtn && saveBtn.parentNode) {
        saveBtn.parentNode.insertBefore(
          cancelBtn,
          saveBtn.nextSibling
        );
      } else {
        form.appendChild(cancelBtn);
      }

      cancelBtn.addEventListener("click", cancelEdit);
    }

    cancelBtn.style.display = "inline-block";

    form.scrollIntoView({
      behavior: "smooth",
      block: "start"
    });
  }

  alert("Story edit mode mein aa gayi hai. Changes karke Update Story dabao.");
}

// ===== CANCEL EDIT =====

function cancelEdit() {
  editingId = null;

  const form =
    document.getElementById("storyForm") ||
    document.getElementById("addForm");

  if (form) form.reset();

  const saveBtn =
    document.getElementById("saveBtn") ||
    document.getElementById("publishBtn") ||
    document.querySelector("#storyForm button[type='submit']") ||
    document.querySelector("#addForm button[type='submit']");

  if (saveBtn) {
    saveBtn.textContent = "Publish Story";
  }

  const cancelBtn = document.getElementById("cancelEditBtn");

  if (cancelBtn) {
    cancelBtn.style.display = "none";
  }
}

// ===== ADD OR UPDATE STORY =====

async function addStory(e) {
  if (e) e.preventDefault();

  const get = id => {
    const el = document.getElementById(id);
    return el ? el.value.trim() : "";
  };

  const payload = {
    title: get("title"),
    category: get("category"),
    country: get("country"),
    date: get("date"),
    story: get("story"),
    image: get("image"),
    video: get("video")
  };

  if (!payload.title) {
    alert("Title likho");
    return;
  }

  try {
    const isEditing = editingId !== null;

    const url = isEditing
      ? API + "/api/stories/" + encodeURIComponent(editingId)
      : API + "/api/stories";

    const res = await fetch(url, {
      method: isEditing ? "PUT" : "POST",
      headers: {
        "Content-Type": "text/plain",
        "Authorization": "Bearer " + token
      },
      body: JSON.stringify(payload)
    });

    const data = await res.json().catch(() => ({}));

    if (!res.ok) {
      alert(
        (isEditing ? "Update" : "Save") +
        " nahi hua: " +
        (data.error || "Please log in again")
      );

      if (res.status === 401) doLogout();

      return;
    }

    alert(isEditing ? "Story update ho gayi!" : "Story save ho gayi!");

    cancelEdit();

    await loadStories();

  } catch (err) {
    alert("Failed: " + err.message);
  }
}

// ===== DELETE STORY =====

async function deleteStory(id) {
  if (!confirm("Is story ko delete karna hai?")) return;

  try {
    const res = await fetch(
      API + "/api/stories/" + encodeURIComponent(id),
      {
        method: "DELETE",
        headers: {
          "Authorization": "Bearer " + token
        }
      }
    );

    const data = await res.json().catch(() => ({}));

    if (!res.ok) {
      alert("Delete nahi hua: " + (data.error || ""));
      return;
    }

    if (String(editingId) === String(id)) {
      cancelEdit();
    }

    alert("Delete ho gayi!");

    loadStories();

  } catch (e) {
    alert("Delete failed: " + e.message);
  }
}

// ===== LOGOUT =====

function doLogout() {
  token = "";
  editingId = null;

  localStorage.removeItem("vv_token");

  showDashboard(false);
}

// ===== CONNECT BUTTONS =====

document.addEventListener("DOMContentLoaded", () => {

  const loginForm = document.getElementById("loginForm");

  if (loginForm) {
    loginForm.addEventListener("submit", doLogin);
  }

  const loginBtn = document.getElementById("loginBtn");

  if (loginBtn && !loginForm) {
    loginBtn.addEventListener("click", doLogin);
  }

  const storyForm =
    document.getElementById("storyForm") ||
    document.getElementById("addForm");

  if (storyForm) {
    storyForm.addEventListener("submit", addStory);
  }

  const logoutBtn = document.getElementById("logoutBtn");

  if (logoutBtn) {
    logoutBtn.addEventListener("click", doLogout);
  }

  if (token) {
    showDashboard(true);
  } else {
    showDashboard(false);
  }

});
```

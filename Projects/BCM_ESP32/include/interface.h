#ifndef INTERFACE_H
#define INTERFACE_H

const char HTML_PAGE[] PROGMEM = R"==(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>BCM Volkswagen</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#1e3a8a 0%,#1e1b4b 100%);color:#fff;padding:20px;min-height:100vh}
.container{max-width:1200px;margin:0 auto}
.header{text-align:center;margin-bottom:40px;padding:30px 0;border-bottom:2px solid #3b82f6}
.header h1{font-size:2.5em;margin-bottom:10px}
.header p{font-size:0.9em;color:#cbd5e1}
.status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-bottom:40px}
.status-item{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:8px;padding:15px;text-align:center}
.status-label{font-size:0.85em;color:#cbd5e1}
.status-value{font-size:1.3em;font-weight:600;color:#3b82f6}
.status-value.active{color:#10b981}
.status-value.inactive{color:#ef4444}
.log-container{background:rgba(0,0,0,0.4);border:1px solid rgba(59,130,246,0.3);border-radius:8px;padding:15px;height:250px;overflow-y:auto;margin-bottom:40px;font-family:'Courier New',monospace;font-size:0.85em}
.log-entry{padding:5px 0;border-bottom:1px solid rgba(255,255,255,0.05);color:#a1a1a1}
.sections{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:40px}
.section{background:rgba(255,255,255,0.05);border:1px solid rgba(59,130,246,0.3);border-radius:12px;padding:20px}
.section-title{font-size:1.1em;font-weight:600;margin-bottom:15px;padding-bottom:10px;border-bottom:2px solid #3b82f6;text-transform:uppercase;letter-spacing:1px}
.section-content{display:flex;flex-direction:column;gap:15px}
.light-option{display:grid;grid-template-columns:100px 1fr;align-items:center;gap:10px}
.light-label{font-size:0.9em;color:#cbd5e1}
.radio-group{display:flex;gap:8px}
.radio-option{flex:1}
.radio-option input[type="radio"]{display:none}
.radio-option label{display:block;padding:8px 12px;border:1px solid rgba(255,255,255,0.2);border-radius:4px;text-align:center;cursor:pointer;font-size:0.85em;background:rgba(255,255,255,0.05)}
.radio-option input[type="radio"]:checked+label{background:#3b82f6;border-color:#3b82f6;color:#fff;font-weight:600}
.duration-input{display:grid;grid-template-columns:100px 1fr;align-items:center;gap:10px}
.duration-input input{padding:8px 12px;border:1px solid rgba(59,130,246,0.5);border-radius:4px;background:rgba(255,255,255,0.05);color:#fff}
.button-group{display:flex;gap:15px;margin-top:30px;justify-content:center}
button{padding:12px 30px;border:none;border-radius:6px;font-size:1em;font-weight:600;cursor:pointer;text-transform:uppercase;transition:all 0.3s ease}
.btn-save{background:linear-gradient(135deg,#10b981 0%,#059669 100%);color:white}
.btn-save:hover{transform:translateY(-2px)}
.btn-reset{background:rgba(59,130,246,0.2);color:#3b82f6;border:1px solid #3b82f6}
.message{text-align:center;padding:12px;border-radius:6px;margin-bottom:20px;display:none}
.message.success{background:rgba(16,185,129,0.2);border:1px solid #10b981;color:#10b981}
.message.error{background:rgba(239,68,68,0.2);border:1px solid #ef4444;color:#ef4444}
</style>
</head>
<body>
<div class="container">
<div class="header">
<h1>BCM Volkswagen</h1>
<p>Sistema de Iluminacao Inteligente</p>
</div>
<div class="message" id="message"></div>
<div class="status-grid">
<div class="status-item"><div class="status-label">Porta</div><div class="status-value" id="door">●</div></div>
<div class="status-item"><div class="status-label">Trava</div><div class="status-value" id="lock">●</div></div>
<div class="status-item"><div class="status-label">Destrava</div><div class="status-value" id="unlock">●</div></div>
<div class="status-item"><div class="status-label">Ignicao</div><div class="status-value" id="ignition">●</div></div>
<div class="status-item"><div class="status-label">Farol</div><div class="status-value" id="headlight">●</div></div>
<div class="status-item"><div class="status-label">DRL</div><div class="status-value" id="drl">●</div></div>
<div class="status-item"><div class="status-label">Interna</div><div class="status-value" id="interior">●</div></div>
<div class="status-item"><div class="status-label">Pes</div><div class="status-value" id="foot">●</div></div>
</div>
<div class="log-container" id="logContainer"></div>
<div class="sections">
<div class="section">
<div class="section-title">Destravar (Welcome Light)</div>
<div class="section-content">
<div class="light-option"><label class="light-label">Farol</label><div class="radio-group"><div class="radio-option"><input type="radio" name="welcomeHeadlight" value="0" id="wh0"><label for="wh0">Desligar</label></div><div class="radio-option"><input type="radio" name="welcomeHeadlight" value="1" id="wh1"><label for="wh1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">DRL</label><div class="radio-group"><div class="radio-option"><input type="radio" name="welcomeDRL" value="0" id="wd0"><label for="wd0">Desligar</label></div><div class="radio-option"><input type="radio" name="welcomeDRL" value="1" id="wd1"><label for="wd1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Interna</label><div class="radio-group"><div class="radio-option"><input type="radio" name="welcomeInterior" value="0" id="wi0"><label for="wi0">Desligar</label></div><div class="radio-option"><input type="radio" name="welcomeInterior" value="1" id="wi1"><label for="wi1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Pes</label><div class="radio-group"><div class="radio-option"><input type="radio" name="welcomeFoot" value="0" id="wf0"><label for="wf0">Desligar</label></div><div class="radio-option"><input type="radio" name="welcomeFoot" value="1" id="wf1"><label for="wf1">Ligar</label></div></div></div>
<div class="duration-input"><label for="welcomeDuration">Duracao (s)</label><input type="number" id="welcomeDuration" min="1" max="300"></div>
</div>
</div>
<div class="section">
<div class="section-title">Travar (Follow Me Home)</div>
<div class="section-content">
<div class="light-option"><label class="light-label">Farol</label><div class="radio-group"><div class="radio-option"><input type="radio" name="followMeHeadlight" value="0" id="fh0"><label for="fh0">Desligar</label></div><div class="radio-option"><input type="radio" name="followMeHeadlight" value="1" id="fh1"><label for="fh1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">DRL</label><div class="radio-group"><div class="radio-option"><input type="radio" name="followMeDRL" value="0" id="fd0"><label for="fd0">Desligar</label></div><div class="radio-option"><input type="radio" name="followMeDRL" value="1" id="fd1"><label for="fd1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Interna</label><div class="radio-group"><div class="radio-option"><input type="radio" name="followMeInterior" value="0" id="fi0"><label for="fi0">Desligar</label></div><div class="radio-option"><input type="radio" name="followMeInterior" value="1" id="fi1"><label for="fi1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Pes</label><div class="radio-group"><div class="radio-option"><input type="radio" name="followMeFoot" value="0" id="ff0"><label for="ff0">Desligar</label></div><div class="radio-option"><input type="radio" name="followMeFoot" value="1" id="ff1"><label for="ff1">Ligar</label></div></div></div>
<div class="duration-input"><label for="followMeDuration">Duracao (s)</label><input type="number" id="followMeDuration" min="1" max="600"></div>
</div>
</div>
<div class="section">
<div class="section-title">Ignicao OFF (Goodbye Light)</div>
<div class="section-content">
<div class="light-option"><label class="light-label">Farol</label><div class="radio-group"><div class="radio-option"><input type="radio" name="goodbyeHeadlight" value="0" id="gh0"><label for="gh0">Desligar</label></div><div class="radio-option"><input type="radio" name="goodbyeHeadlight" value="1" id="gh1"><label for="gh1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">DRL</label><div class="radio-group"><div class="radio-option"><input type="radio" name="goodbyeDRL" value="0" id="gd0"><label for="gd0">Desligar</label></div><div class="radio-option"><input type="radio" name="goodbyeDRL" value="1" id="gd1"><label for="gd1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Interna</label><div class="radio-group"><div class="radio-option"><input type="radio" name="goodbyeInterior" value="0" id="gi0"><label for="gi0">Desligar</label></div><div class="radio-option"><input type="radio" name="goodbyeInterior" value="1" id="gi1"><label for="gi1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Pes</label><div class="radio-group"><div class="radio-option"><input type="radio" name="goodbyeFoot" value="0" id="gf0"><label for="gf0">Desligar</label></div><div class="radio-option"><input type="radio" name="goodbyeFoot" value="1" id="gf1"><label for="gf1">Ligar</label></div></div></div>
<div class="duration-input"><label for="goodbyeDuration">Duracao (s)</label><input type="number" id="goodbyeDuration" min="1" max="300"></div>
</div>
</div>
<div class="section">
<div class="section-title">Ignicao Ligada</div>
<div class="section-content">
<div class="light-option"><label class="light-label">Farol</label><div class="radio-group"><div class="radio-option"><input type="radio" name="ignitionOnHeadlight" value="0" id="ih0"><label for="ih0">Desligar</label></div><div class="radio-option"><input type="radio" name="ignitionOnHeadlight" value="1" id="ih1"><label for="ih1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">DRL</label><div class="radio-group"><div class="radio-option"><input type="radio" name="ignitionOnDRL" value="0" id="id0"><label for="id0">Desligar</label></div><div class="radio-option"><input type="radio" name="ignitionOnDRL" value="1" id="id1"><label for="id1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Interna</label><div class="radio-group"><div class="radio-option"><input type="radio" name="ignitionOnInterior" value="0" id="ii0"><label for="ii0">Desligar</label></div><div class="radio-option"><input type="radio" name="ignitionOnInterior" value="1" id="ii1"><label for="ii1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Pes</label><div class="radio-group"><div class="radio-option"><input type="radio" name="ignitionOnFoot" value="0" id="ifo0"><label for="ifo0">Desligar</label></div><div class="radio-option"><input type="radio" name="ignitionOnFoot" value="1" id="ifo1"><label for="ifo1">Ligar</label></div></div></div>
<div class="duration-input"><label for="ignitionOnDuration">Duracao (s)</label><input type="number" id="ignitionOnDuration" min="1" max="300"></div>
</div>
</div>
<div class="section">
<div class="section-title">Porta Aberta</div>
<div class="section-content">
<div class="light-option"><label class="light-label">Farol</label><div class="radio-group"><div class="radio-option"><input type="radio" name="doorOpenHeadlight" value="0" id="doh0"><label for="doh0">Desligar</label></div><div class="radio-option"><input type="radio" name="doorOpenHeadlight" value="1" id="doh1"><label for="doh1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">DRL</label><div class="radio-group"><div class="radio-option"><input type="radio" name="doorOpenDRL" value="0" id="dod0"><label for="dod0">Desligar</label></div><div class="radio-option"><input type="radio" name="doorOpenDRL" value="1" id="dod1"><label for="dod1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Interna</label><div class="radio-group"><div class="radio-option"><input type="radio" name="doorOpenInterior" value="0" id="doi0"><label for="doi0">Desligar</label></div><div class="radio-option"><input type="radio" name="doorOpenInterior" value="1" id="doi1"><label for="doi1">Ligar</label></div></div></div>
<div class="light-option"><label class="light-label">Pes</label><div class="radio-group"><div class="radio-option"><input type="radio" name="doorOpenFoot" value="0" id="dof0"><label for="dof0">Desligar</label></div><div class="radio-option"><input type="radio" name="doorOpenFoot" value="1" id="dof1"><label for="dof1">Ligar</label></div></div></div>
<div class="duration-input"><label for="doorOpenDuration">Duracao (s)</label><input type="number" id="doorOpenDuration" min="1" max="300"></div>
</div>
</div>
</div>
<div class="button-group">
<button class="btn-save" onclick="saveConfig()">Salvar Configuracao</button>
<button class="btn-reset" onclick="loadConfig()">Recarregar</button>
</div>
</div>
<script>
const s={door:document.getElementById('door'),lock:document.getElementById('lock'),unlock:document.getElementById('unlock'),ignition:document.getElementById('ignition'),headlight:document.getElementById('headlight'),drl:document.getElementById('drl'),interior:document.getElementById('interior'),foot:document.getElementById('foot')};
function loadConfig(){fetch('/getConfig').then(r=>r.json()).then(d=>{document.querySelector('input[name="welcomeHeadlight"][value="'+d.welcomeHeadlight+'"]').checked=true;document.querySelector('input[name="welcomeDRL"][value="'+d.welcomeDRL+'"]').checked=true;document.querySelector('input[name="welcomeInterior"][value="'+d.welcomeInterior+'"]').checked=true;document.querySelector('input[name="welcomeFoot"][value="'+d.welcomeFoot+'"]').checked=true;document.getElementById('welcomeDuration').value=d.welcomeDuration;document.querySelector('input[name="goodbyeHeadlight"][value="'+d.goodbyeHeadlight+'"]').checked=true;document.querySelector('input[name="goodbyeDRL"][value="'+d.goodbyeDRL+'"]').checked=true;document.querySelector('input[name="goodbyeInterior"][value="'+d.goodbyeInterior+'"]').checked=true;document.querySelector('input[name="goodbyeFoot"][value="'+d.goodbyeFoot+'"]').checked=true;document.getElementById('goodbyeDuration').value=d.goodbyeDuration;document.querySelector('input[name="followMeHeadlight"][value="'+d.followMeHeadlight+'"]').checked=true;document.querySelector('input[name="followMeDRL"][value="'+d.followMeDRL+'"]').checked=true;document.querySelector('input[name="followMeInterior"][value="'+d.followMeInterior+'"]').checked=true;document.querySelector('input[name="followMeFoot"][value="'+d.followMeFoot+'"]').checked=true;document.getElementById('followMeDuration').value=d.followMeDuration;document.querySelector('input[name="doorOpenHeadlight"][value="'+d.doorOpenHeadlight+'"]').checked=true;document.querySelector('input[name="doorOpenDRL"][value="'+d.doorOpenDRL+'"]').checked=true;document.querySelector('input[name="doorOpenInterior"][value="'+d.doorOpenInterior+'"]').checked=true;document.querySelector('input[name="doorOpenFoot"][value="'+d.doorOpenFoot+'"]').checked=true;document.getElementById('doorOpenDuration').value=d.doorOpenDuration;document.querySelector('input[name="ignitionOnHeadlight"][value="'+d.ignitionOnHeadlight+'"]').checked=true;document.querySelector('input[name="ignitionOnDRL"][value="'+d.ignitionOnDRL+'"]').checked=true;document.querySelector('input[name="ignitionOnInterior"][value="'+d.ignitionOnInterior+'"]').checked=true;document.querySelector('input[name="ignitionOnFoot"][value="'+d.ignitionOnFoot+'"]').checked=true;document.getElementById('ignitionOnDuration').value=d.ignitionOnDuration;}).catch(e=>console.error('Erro:',e))}
function saveConfig(){const c={welcomeHeadlight:parseInt(document.querySelector('input[name="welcomeHeadlight"]:checked').value),welcomeDRL:parseInt(document.querySelector('input[name="welcomeDRL"]:checked').value),welcomeInterior:parseInt(document.querySelector('input[name="welcomeInterior"]:checked').value),welcomeFoot:parseInt(document.querySelector('input[name="welcomeFoot"]:checked').value),welcomeDuration:parseInt(document.getElementById('welcomeDuration').value),goodbyeHeadlight:parseInt(document.querySelector('input[name="goodbyeHeadlight"]:checked').value),goodbyeDRL:parseInt(document.querySelector('input[name="goodbyeDRL"]:checked').value),goodbyeInterior:parseInt(document.querySelector('input[name="goodbyeInterior"]:checked').value),goodbyeFoot:parseInt(document.querySelector('input[name="goodbyeFoot"]:checked').value),goodbyeDuration:parseInt(document.getElementById('goodbyeDuration').value),followMeHeadlight:parseInt(document.querySelector('input[name="followMeHeadlight"]:checked').value),followMeDRL:parseInt(document.querySelector('input[name="followMeDRL"]:checked').value),followMeInterior:parseInt(document.querySelector('input[name="followMeInterior"]:checked').value),followMeFoot:parseInt(document.querySelector('input[name="followMeFoot"]:checked').value),followMeDuration:parseInt(document.getElementById('followMeDuration').value),doorOpenHeadlight:parseInt(document.querySelector('input[name="doorOpenHeadlight"]:checked').value),doorOpenDRL:parseInt(document.querySelector('input[name="doorOpenDRL"]:checked').value),doorOpenInterior:parseInt(document.querySelector('input[name="doorOpenInterior"]:checked').value),doorOpenFoot:parseInt(document.querySelector('input[name="doorOpenFoot"]:checked').value),doorOpenDuration:parseInt(document.getElementById('doorOpenDuration').value),ignitionOnHeadlight:parseInt(document.querySelector('input[name="ignitionOnHeadlight"]:checked').value),ignitionOnDRL:parseInt(document.querySelector('input[name="ignitionOnDRL"]:checked').value),ignitionOnInterior:parseInt(document.querySelector('input[name="ignitionOnInterior"]:checked').value),ignitionOnFoot:parseInt(document.querySelector('input[name="ignitionOnFoot"]:checked').value),ignitionOnDuration:parseInt(document.getElementById('ignitionOnDuration').value)};fetch('/saveConfig',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(c)}).then(r=>r.json()).then(r=>{showMessage(r.success?'Salvo!':'Erro','success')}).catch(e=>{showMessage('Erro','error')})}
function showMessage(t,y){const m=document.getElementById('message');m.textContent=t;m.className='message '+y;m.style.display='block';setTimeout(()=>m.style.display='none',3000)}
function updateStatus(){fetch('/getStatus').then(r=>r.json()).then(d=>{s.door.textContent=d.door?'ON':'OFF';s.door.className='status-value '+(d.door?'active':'inactive');s.lock.textContent=d.lock?'ON':'OFF';s.lock.className='status-value '+(d.lock?'active':'inactive');s.unlock.textContent=d.unlock?'ON':'OFF';s.unlock.className='status-value '+(d.unlock?'active':'inactive');s.ignition.textContent=d.ignition?'ON':'OFF';s.ignition.className='status-value '+(d.ignition?'active':'inactive');s.headlight.textContent=d.headlight?'ON':'OFF';s.headlight.className='status-value '+(d.headlight?'active':'inactive');s.drl.textContent=d.drl?'ON':'OFF';s.drl.className='status-value '+(d.drl?'active':'inactive');s.interior.textContent=d.interior?'ON':'OFF';s.interior.className='status-value '+(d.interior?'active':'inactive');s.foot.textContent=d.foot?'ON':'OFF';s.foot.className='status-value '+(d.foot?'active':'inactive');const l=document.getElementById('logContainer');l.innerHTML=d.logs.map(g=>'<div class="log-entry">'+g+'</div>').join('');l.scrollTop=l.scrollHeight}).catch(e=>console.error('Error:',e))}
loadConfig();updateStatus();setInterval(updateStatus,500);
</script>
</body>
</html>
)==";

#endif

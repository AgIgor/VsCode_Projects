const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>ESP32 CLP</title>
<style>
body { font-family: Arial; background:#111; color:#eee; }
select,button { font-size:16px; }
</style>
</head>
<body>
<h2>ESP32 CLP – Ladder simples</h2>

<select id="i1">
  <option value="0">I0</option>
  <option value="1">I1</option>
</select>

<select id="op">
  <option value="AND">AND</option>
  <option value="OR">OR</option>
</select>

<select id="i2">
  <option value="0">I0</option>
  <option value="1">I1</option>
</select>

<br><br>
<button onclick="send()">Enviar programa</button>

<script>
function send() {
  const prog = {
    rungs: [{
      logic: [
        {type:"IN",id:parseInt(i1.value)},
        {type:"IN",id:parseInt(i2.value)},
        {type:op.value}
      ],
      timer:{type:"TON",pt:3000},
      output:0
    }]
  };

  fetch("/program",{
    method:"POST",
    body:JSON.stringify(prog)
  });
}
</script>
</body>
</html>
)rawliteral";

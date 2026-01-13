<!-- Instruções específicas para agentes de código (Copilot / AI assistants).
     Objetivo: fornecer orientação prática e reproduzível para trabalhar neste projeto
     sem necessidade de conhecimento externo. -->

# Copilot instructions — EspNow (PlatformIO / ESP32)

Resumo rápido
- Repositório: projeto PlatformIO para ESP32 usando ESP-NOW (arquivo principal: `src/main.cpp`).
- Alvo padrão: ambiente `env:esp32doit-devkit-v1` (veja `platformio.ini`).

O que é importante saber
- Arquivo central: `src/main.cpp` contém a implementação mestre/escravo ESP-NOW.
  - Mensagem definida por `espnow_message_t` com campos: `dest_id`, `src_id`, `seq`, `cmd`, `len`, `payload`.
  - Inicialização ESP-NOW: `initEspNow()` chama `esp_now_init()` e registra callbacks `onDataRecv` e `onDataSent`.
  - Papel (MASTER/SLAVE) é decidido via entrada Serial no boot; mestre usa `peers[]` (MAC -> node_id).

Arquivos e locais-chave
- `platformio.ini` — ambiente `esp32doit-devkit-v1`, monitor em `115200`.
- `src/main.cpp` — lógica completa: parsing serial, adicionar peers (`addPeerMac`), funções de envio `sendToNodeId`.
- `include/README`, `lib/README`, `test/README` — arquivos gerados pelo template PlatformIO (informativos).

Comandos de build / upload / debug (PlatformIO)
- Build: `pio run`
- Upload: `pio run -t upload` (usa env configurado no `platformio.ini`)
- Monitor serial: `pio device monitor -b 115200`

Padrões e convenções do projeto
- IDs de nós: 0 = mestre (convenção no código). Escravos usam IDs 1..254.
- Mapeamento: o mestre mantém `Peer peers[]` no código fonte; procuro por peers estáticos e uso MACs literais.
- Formato MAC: `AA:BB:CC:DD:EE:FF` — o código usa `sscanf` para parsear quando informado via Serial.
- Payload: `MAX_PAYLOAD` está definido em `main.cpp` (200 bytes por padrão).
- Broadcast: use `dest_id == 0xFF` para mensagens aceitas por todos.

Como testar / exemplo de fluxo
1. Em cada ESP32, abra monitor serial (`pio device monitor`) e escolha `m` (MASTER) ou `s` (SLAVE) quando solicitado.
2. Em escravos: informe o `nodeId` (1..254) e, opcionalmente, MAC do mestre (formato `AA:BB:CC:DD:EE:FF`) para permitir respostas.
3. No mestre: atualize `peers[]` em `src/main.cpp` com entradas do tipo `{ {0x24,0x6F,0x28,0xAA,0xBB,0xCC}, 1 }` e recompile/upload.
4. Use o monitor serial do mestre e execute:
   - `list` — exibe peers conhecidos
   - `send 1 Olá` — envia a string `Olá` para o nó com `node_id` 1

Dicas práticas e pontos a observar
- Descoberta de MAC: para obter o MAC de um ESP32 rode um firmware simples que imprime `WiFi.macAddress()` no boot.
- `esp_now_add_peer()` é usado em `addPeerMac()` — se o peer já existir, a função retorna true.
- A implementação atual envia a estrutura inteira (`sizeof(espnow_message_t)`) via `esp_now_send()` — receptores copiam e processam apenas `len` bytes de payload.

Possíveis melhorias (apenas referências, não implementadas automaticamente)
- Persistir `peers[]` em LittleFS/SPIFFS em vez de recompilar (útil para rede dinâmica).
- Implementar ACK + retransmissão: usar `cmd` para diferenciar ACKs e adicionar timeout/retry no mestre.
- Separar builds: criar `src/master.cpp` e `src/slave.cpp` com `platformio.ini` multi-env para evitar prompt Serial em vários nós.

Onde um assistente deve tomar decisões automáticas
- Ao adicionar/editar peers, preferir não sobrescrever `peers[]` sem confirmação humana (evitar que MACs reais sejam perdidos).
- Para mudanças de protocolo (ex.: reduzir `MAX_PAYLOAD`), atualizar tanto `src/main.cpp` quanto documentação.

Pesquisa rápida no código
- Callbacks: `onDataRecv`, `onDataSent` em `src/main.cpp` — modificar aqui para alterar comportamento de recepção/envio.
- Funções de envio: `sendToNodeId`, `sendToMac` — ponto central para qualquer transformação de payload.

Se algo estiver faltando
- Peça ao mantenedor que inclua exemplos reais de MACs em `peers[]` ou um arquivo `peers.json` para discovery automatizado.

Fim — peça feedback ao autor sobre dúvidas ou cases não cobertos por esta instrução.

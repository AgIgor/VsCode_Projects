# Guia de Início Rápido - Automatizador de Luzes Automotivo

## 🚀 Passos para Começar

### 1. Verificar Instalação do PlatformIO
```powershell
platformio --version
```

### 2. Compilar o Projeto
```powershell
cd 'c:\Users\Igor\Documents\PlatformIO\Projects\Automatizador_Luzes_Automotivo'
platformio run
```

### 3. Fazer Upload para o ESP32
```powershell
# Conecte o ESP32 via USB primeiro
platformio run --target upload
```

### 4. Monitorar Serial Output
```powershell
platformio device monitor --baud 115200
```

Você verá:
```
=== Automatizador de Luzes Automotivo (ESP32) ===
Iniciando...

LittleFS inicializado com sucesso
Configuração carregada com sucesso
GPIO inicializado
Servidor web iniciado
Acesse o painel em: http://192.168.4.1 ou http://luz-auto.local

Sistema pronto!
```

### 5. Acessar Interface Web
1. Procure pela rede WiFi: **LuzesAuto**
2. Senha: **12345678**
3. Abra navegador: **http://192.168.4.1**

## 📱 Interface Web Disponível

Após conectar, você terá acesso às abas:

- ⏱️ **Tempos** - Configurar duração dos modos
- 📥 **Entradas** - Mapear GPIO de sensores
- 📤 **Saídas** - Mapear GPIO de atuadores
- 💡 **Sensor de Luz** - Ajustar LDR
- ⚙️ **Features** - Ativar/desativar modos
- 🔧 **Sistema** - Reset e informações

## 🔌 Pinagem Padrão (ESP32)

| Componente | GPIO | Tipo |
|-----------|------|------|
| Trava | 4 | Input Digital |
| Seta | 5 | Input Digital |
| Porta | 6 | Input Digital |
| Ignição | 7 | Input Digital |
| Destrava | 8 | Input Digital |
| Sensor Luz (LDR) | 35 | Entrada ADC |
| Farol | 2 | Output Digital |
| Acessório | 3 | Output Digital |

**Todos podem ser mudados via interface web!**

## 🛠️ Comandos Úteis

### Limpar Build Anterior
```powershell
platformio run --target clean
```

### Rebuild Completo
```powershell
platformio run --target clean
platformio run --target upload
```

### Ver Informações da Placa
```powershell
platformio boards esp32doit-devkit-v1
```

### Ver Portas Disponíveis
```powershell
platformio device list
```

## ⚡ Troubleshooting Rápido

**Erro: "Failed to find serial monitor port"**
- Conecte o ESP32 via USB
- Aguarde drivers instalarem
- Tente novamente

**Erro: "Not enough space"**
```powershell
platformio run --target clean
```

**WiFi não conecta**
- Reset ESP32: segure RESET + 2s
- Verifique se LittleFS foi gravado

**Interface web em branco**
- F5 para recarregar (hard reload: Ctrl+Shift+R)
- Verifique console (F12) para erros
- Reset ESP32

## 📊 Status em Tempo Real

Abra o Serial Monitor para ver o status a cada 5 segundos:

```
=== System Status ===
Uptime: 125 seconds
--- Inputs ---
Lock: HIGH
Turn Signal: LOW
Door: OPEN
Ignition: HIGH
Unlock: HIGH
LDR Value: 850
--- Outputs ---
Headlight: ON
Accessory: OFF
Free Heap: 204.5 KB
====================
```

## 🌐 API REST (Exemplo)

### Obter Configuração Atual
```bash
curl http://192.168.4.1/api/config
```

### Salvar Nova Configuração
```bash
curl -X POST http://192.168.4.1/api/config \
  -H "Content-Type: application/json" \
  -d '{"debounceMs": 50, "ldrDarkThreshold": 800}'
```

### Ver Status do Sistema
```bash
curl http://192.168.4.1/api/status
```

### Resetar para Padrões
```bash
curl -X POST http://192.168.4.1/api/reset
```

## 💾 Arquivos Importantes

- `platformio.ini` - Configuração do projeto
- `src/main.cpp` - Código principal
- `include/ConfigManager.h` - Gerenciamento de config
- `include/WebServer.h` - Servidor e interface web
- `include/PinConfig.h` - Definição de pinos
- `config.example.json` - Exemplo de formato JSON
- `README.md` - Documentação completa

## 📝 Próximas Personalizações

1. **Alterar Senha WiFi**: Em `include/PinConfig.h`
   ```cpp
   const char* WIFI_PASSWORD = "sua_senha";
   ```

2. **Mudar Pino Padrão**: Via interface web ou em `include/PinConfig.h`

3. **Adicionar Novo Parâmetro**:
   - Adicione em `SystemConfig` (ConfigManager.h)
   - Carregue/salve no JSON
   - Exponha na API
   - Adicione no HTML

## ✅ Checklist de Instalação

- [ ] PlatformIO instalado
- [ ] ESP32 conectado via USB
- [ ] Projeto clona/abre no VS Code
- [ ] `platformio run` compila sem erros
- [ ] Upload feito com sucesso
- [ ] Serial monitor mostra "Sistema pronto!"
- [ ] WiFi "LuzesAuto" aparece
- [ ] Interface web carrega em http://192.168.4.1
- [ ] Consegue salvar configurações
- [ ] Status atualiza em tempo real

## 🎉 Pronto!

Seu sistema de automação está funcionando! Explore as abas e customize conforme necessário.

Para dúvidas, consulte README.md completo.

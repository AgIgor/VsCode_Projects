# 🚗 Sistema de Controle Inteligente de Luzes

Sistema completo de automação de luzes para veículos com interface web, motor de regras programável e simulador em tempo real para ESP32.

## 🆕 Novidades v1.1
- ✅ **Suporte a "SENAO" (else)** - Defina ações alternativas
- ✅ **Saídas como condições** - Leia o estado das saídas nas regras
- ✅ **Controle manual** - Botões ON/OFF para cada saída na interface
- ✅ **Sincronização bidirecional** - Interface ↔ ESP32 em tempo real

📖 [Ver novidades detalhadas](NOVIDADES.md)

## 📋 Características

- ✅ **4 Entradas digitais** (pull-up): Trava, Destrava, Porta, Ignição
- ✅ **4 Saídas digitais**: Luz Interna, Luz Assoalho, Farol, DRL
- ✅ **Motor de regras programável** (texto ou visual)
- ✅ **Simulador web em tempo real**
- ✅ **Suporte a timers** (delay e duração)
- ✅ **Operadores lógicos** (AND, OR)
- ✅ **Interface mobile-friendly**
- ✅ **WebSocket** para comunicação em tempo real
- ✅ **Detecção de pulso** para trava/destrava
- ✅ **Toggle** para porta/ignição

## 🔌 Pinagem

### Entradas (Pull-up)
- **Trava**: GPIO 12
- **Destrava**: GPIO 13
- **Porta**: GPIO 14
- **Ignição**: GPIO 27

### Saídas
- **Luz Interna**: GPIO 25
- **Luz Assoalho**: GPIO 26
- **Farol**: GPIO 32
- **DRL**: GPIO 33

## 📝 Sintaxe de Regras

### Estrutura Básica
```
[tipo] [condições] entao [ação]
```

### Tipos de Regra
- `se` - Executa uma vez quando a condição é verdadeira
- `quando` - Executa ao mudar de estado
- `enquanto` - Mantém o estado enquanto a condição for verdadeira

### Condições
```
entrada == estado [operador entrada == estado]
```

**Entradas disponíveis:**
- `trava`
- `destrava`
- `porta`
- `ignicao`

**Estados:**
- `low` - Entrada em nível baixo
- `high` - Entrada em nível alto

**Operadores lógicos:**
- `and` ou `e` - E lógico
- `or` ou `ou` - OU lógico

### Ações
```
saida == estado [em Xs] [por Ys]
```

**Saídas disponíveis:**
- `luz-interna`
- `luz-assoalho`
- `farol`
- `drl`

**Modificadores:**
- `em Xs` - Atraso de X segundos antes de executar
- `por Ys` - Duração de Y segundos (retorna ao estado oposto após)

## 💡 Exemplos de Regras

### Básicas
```
se porta == low entao luz-interna == high
se ignicao == high entao farol == high
quando trava == low entao drl == high por 5s
```

### Com Timers
```
se porta == low entao luz-interna == high em 2s
se ignicao == low entao farol == high por 15s
se trava == low entao luz-assoalho == high em 1s por 30s
```

### Com Operadores Lógicos
```
se porta == low and ignicao == low entao luz-interna == high
se ignicao == high or porta == low entao luz-assoalho == high
enquanto porta == low and ignicao == low entao farol == high
```

### Complexas
```
se trava == low and ignicao == high entao drl == high em 2s por 60s
quando porta == low or destrava == low entao luz-interna == high por 10s
enquanto ignicao == high and porta == high entao farol == high
```

### Com "SENAO" - Ações Alternativas (v1.1+)
```
se porta == low entao luz-interna == high senao luz-interna == low
se ignicao == high entao farol == high em 2s senao farol == low
se destrava == low entao luz-assoalho == high por 30s senao luz-assoalho == low
```

### Usando Saídas como Condições (v1.1+)
```
se farol == high entao drl == low
se luz-interna == high entao luz-assoalho == high em 2s senao luz-assoalho == low
se porta == low and luz-interna == low entao luz-interna == high por 20s
```

## 🚀 Como Usar

### 1. Configurar WiFi
Edite no arquivo `src/main.cpp`:
```cpp
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
```

### 2. Compilar e Fazer Upload
```bash
# Compilar o código
pio run

# Fazer upload para o ESP32
pio run --target upload

# Fazer upload do sistema de arquivos (HTML)
pio run --target uploadfs
```

### 3. Acessar Interface
- Conecte ao WiFi configurado
- Abra o navegador em: `http://[IP_DO_ESP32]`
- O IP será mostrado no Monitor Serial

### 4. Programar Regras

#### Modo Texto (Recomendado)
1. Clique na aba "Modo Texto"
2. Digite suas regras (uma por linha)
3. Clique em "Aplicar Regras"

#### Modo Visual
1. Clique na aba "Modo Visual"
2. Clique em "+ Adicionar Regra"
3. Configure os parâmetros
4. Clique em "Aplicar Esta Regra"

### 5. Testar Simulador
Use os botões na interface para simular as entradas:
- **PULSO** - Simula um pulso momentâneo (trava/destrava)
- **ABRIR/FECHAR** - Alterna o estado (porta)
- **LIGAR/DESLIGAR** - Alterna o estado (ignição)

Os indicadores das saídas acenderão automaticamente conforme as regras.

## 🛠️ Desenvolvimento

### Estrutura do Projeto
```
modulo_luzes_web/
├── data/
│   └── index.html          # Interface web
├── src/
│   └── main.cpp            # Código ESP32
├── platformio.ini          # Configuração PlatformIO
└── README.md               # Este arquivo
```

### Dependências
- ESP Async WebServer ^1.2.3
- AsyncTCP ^1.1.1
- ArduinoJson ^6.21.3

### Monitor Serial
```bash
pio device monitor
```

## 🐛 Troubleshooting

### ESP32 não conecta ao WiFi
- Verifique SSID e senha
- Tente criar um AP (automático após 20 tentativas falhas)
- Nome do AP: `ESP32-Luzes`
- Senha: `12345678`

### Interface não carrega
- Verifique se fez upload do filesystem: `pio run --target uploadfs`
- Verifique se LittleFS foi montado no Serial Monitor

### Regras não funcionam
- Verifique a sintaxe no console da interface
- Use letras minúsculas
- Certifique-se de usar `==` (dois iguais)
- Verifique espaços ao redor dos operadores

### Saídas não respondem
- Verifique pinagem no código
- Teste com LED externo
- Verifique alimentação adequada para cargas

## 📱 Interface Mobile

A interface é totalmente responsiva e otimizada para smartphones. Acesse pelo navegador do celular conectado na mesma rede WiFi.

## 🔄 Atualizações Futuras

- [ ] Salvar regras em memória não-volátil
- [ ] Editor de regras com syntax highlighting
- [ ] Histórico de eventos
- [] Agendamento por horário
- [ ] Integração com sensores adicionais
- [ ] API REST completa
- [ ] Autenticação de usuário

## 📄 Licença

Este projeto é de código aberto e livre para uso pessoal e comercial.

## 👨‍💻 Autor

Desenvolvido para automação veicular com ESP32.

---

**Versão:** 1.0.0  
**Data:** Fevereiro 2026

# ⚡ GUIA RÁPIDO DE INÍCIO

## 🔧 Passos para Configurar

### 1. Configurar WiFi
Abra `src/main.cpp` e altere as linhas 9-10:

```cpp
const char* ssid = "SEU_WIFI";          // <- Seu WiFi aqui
const char* password = "SUA_SENHA";     // <- Sua senha aqui
```

### 2. Fazer Upload do Código
No terminal do VS Code (PowerShell):

```powershell
# Compilar e fazer upload para o ESP32
C:\Users\Igor\.platformio\penv\Scripts\platformio.exe run --target upload
```

### 3. Fazer Upload dos Arquivos Web (HTML)
```powershell
# Upload do sistema de arquivos (index.html)
C:\Users\Igor\.platformio\penv\Scripts\platformio.exe run --target uploadfs
```

### 4. Monitorar o Serial
```powershell
# Ver mensagens do ESP32
C:\Users\Igor\.platformio\penv\Scripts\platformio.exe device monitor
```

O IP do ESP32 aparecerá no monitor serial após conectar ao WiFi.

### 5. Acessar a Interface
- Abra o navegador
- Digite: `http://[IP_DO_ESP32]`
- Exemplo: `http://192.168.1.100`

---

## 📱 Como Usar o Simulador

### Simulando Entradas

**Botões PULSO** (Trava/Destrava):
- Clique uma vez
- Simula um pulso momentâneo de 300ms
- Ideal para alarmes com pulso

**Botões TOGGLE** (Porta/Ignição):
- Clique para alternar estado
- ABRIR ↔ FECHAR
- LIGAR ↔ DESLIGAR

### Visualizando Saídas

Os **indicadores circulares** mostram o estado:
- 🟢 **Verde brilhante** = HIGH (ligado)
- ⚪ **Cinza** = LOW (desligado)

---

## 📝 Programando Regras

### Modo Texto (Recomendado)

#### Sintaxe Básica
```
[tipo] [condição] entao [ação]
```

#### Exemplos Práticos

**1. Luz de cortesia ao abrir porta:**
```
se porta == low entao luz-interna == high
```

**2. Farol com ignição (com delay):**
```
se ignicao == high entao farol == high em 2s
```

**3. DRL temporizado ao travar:**
```
quando trava == low entao drl == high por 5s
```

**4. Múltiplas condições (AND):**
```
se porta == low and ignicao == low entao luz-interna == high
```

**5. Múltiplas condições (OR):**
```
se porta == low or destrava == low entao luz-assoalho == high
```

**6. Regra complexa com timer:**
```
se trava == low and ignicao == high entao farol == high em 2s por 60s
```

#### Dicas:
- ✅ Use letras **minúsculas**
- ✅ Use `==` (dois iguais)
- ✅ Deixe espaços ao redor dos operadores
- ✅ Uma regra por linha
- ✅ Comentários com `//`

---

## 🎯 Exemplos de Uso Real

### Cenário 1: Sistema Básico de Cortesia
```
// Luz interna ao abrir porta
se porta == low entao luz-interna == high

// Desliga ao fechar porta
se porta == high entao luz-interna == low
```

### Cenário 2: Faróis Automáticos
```
// Liga farol com ignição após 2s
se ignicao == high entao farol == high em 2s

// DRL sempre que carro está ligado
enquanto ignicao == high entao drl == high

// Desliga tudo com ignição desligada
se ignicao == low entao farol == low
se ignicao == low entao drl == low
```

### Cenário 3: Sistema de Alarme
```
// Piscada ao travar (3s)
quando trava == low entao drl == high por 3s

// Piscada ao destravar (3s)
quando destrava == low entao farol == high por 3s
```

### Cenário 4: Luz de Assoalho Inteligente
```
// Liga ao abrir porta se ignição desligada
se porta == low and ignicao == low entao luz-assoalho == high

// Mantém por 30s após fechar porta
se porta == high and ignicao == low entao luz-assoalho == high por 30s
```

### Cenário 5: Sistema Completo
```
// Cortesia ao abrir porta
se porta == low entao luz-interna == high
se porta == low entao luz-assoalho == high

// Farol com ignição
se ignicao == high entao farol == high em 2s

// DRL automático
enquanto ignicao == high entao drl == high

// Piscadas de alarme
quando trava == low entao drl == high por 3s
quando destrava == low entao farol == high por 3s

// Luz temporizada ao destravar
se destrava == low and ignicao == low entao luz-interna == high por 15s
```

---

## 🔌 Conexões Físicas

### Entradas (Pull-up interno ativado)

```
Sensor/Botão          ESP32
   GND    ----------- GND
   Sinal  ----------- GPIO (veja abaixo)
```

| Entrada   | GPIO | Função |
|-----------|------|--------|
| Trava     | 12   | Pulso do alarme (trava) |
| Destrava  | 13   | Pulso do alarme (destrava) |
| Porta     | 14   | Sensor de porta (LOW=aberta) |
| Ignição   | 27   | Positivo da ignição |

### Saídas (Lógica Direta)

```
ESP32                 Relé/LED
GPIO   -----------   IN+ (sinal)
GND    -----------   IN- (comum)
```

| Saída        | GPIO | Carga |
|--------------|------|-------|
| Luz Interna  | 25   | LED interno |
| Luz Assoalho | 26   | LED assoalho |
| Farol        | 32   | Relé farol |
| DRL          | 33   | LED DRL |

⚠️ **IMPORTANTE:** Use relés para cargas acima de 20mA!

---

## 🐛 Solução de Problemas

### ESP32 não conecta ao WiFi
1. Verifique SSID e senha
2. Após 20 tentativas, cria AP automaticamente
3. Conecte ao AP: `ESP32-Luzes` / senha: `12345678`
4. Acesse: `http://192.168.4.1`

### Interface não carrega
1. Certifique-se de fazer upload do filesystem:
   ```powershell
   C:\Users\Igor\.platformio\penv\Scripts\platformio.exe run --target uploadfs
   ```
2. Verifique no serial se LittleFS montou com sucesso

### Regras não funcionam
1. Verifique sintaxe (minúsculas, espaços, `==`)
2. Olhe o console na interface web (mensagens de erro)
3. Teste regras simples primeiro

### Saídas não respondem
1. Verifique pinagem
2. Teste com LED e resistor primeiro
3. Use relés para cargas maiores
4. Verifique alimentação adequada

---

## 📊 Monitoramento

### Console do Serial
```powershell
C:\Users\Igor\.platformio\penv\Scripts\platformio.exe device monitor
```

Mostra:
- Estado das entradas
- Regras ativadas
- Saídas alteradas
- Erros de parsing

### Console da Interface Web
Na parte inferior da interface web:
- Regras carregadas
- Ações executadas
- Timers agendados
- Erros de sintaxe

---

## 🎓 Operadores Disponíveis

### Tipos de Regra
| Tipo | Comportamento |
|------|---------------|
| `se` | Executa UMA VEZ quando condição vira verdadeira |
| `quando` | Executa ao MUDAR de estado |
| `enquanto` | MANTÉM estado enquanto condição for verdadeira |

### Operadores Lógicos
| Operador | Equivalente | Função |
|----------|-------------|--------|
| `and`    | `e`         | E lógico (ambos verdadeiros) |
| `or`     | `ou`        | OU lógico (pelo menos um verdadeiro) |

### Estados de Entrada
| Estado | Significado |
|--------|-------------|
| `low`  | Nível baixo (0V) / Acionado |
| `high` | Nível alto (3.3V) / Desativado |

### Modificadores de Tempo
| Modificador | Efeito |
|-------------|--------|
| `em Xs`     | Espera X segundos antes de executar |
| `por Ys`    | Mantém estado por Y segundos |

---

## 💡 Dicas Pro

1. **Teste no simulador primeiro** - Vale regras sem programar o ESP32
2. **Use comentários** - `// isto é um comentário`
3. **Regras incrementais** - Adicione uma de cada vez
4. **Console é seu amigo** - Sempre monitore o console
5. **Mobile funciona!** - Acesse pelo celular na mesma rede
6. **Salve suas regras** - Copie do editor para um arquivo
7. **Priorize `enquanto`** - Para estados que devem ser mantidos
8. **Combine timers** - `em Xs por Ys` é muito poderoso

---

## 📱 Atalhos de Teclado (Interface)

- `Ctrl + Enter` - Aplicar regras (modo texto)
- `Ctrl + L` - Limpar console
- `Ctrl + S` - Salvar regras (se implementado)

---

## 🚀 Próximos Passos

1. ✅ Configure WiFi e faça upload
2. ✅ Teste o simulador na interface
3. ✅ Crie regras simples
4. ✅ Conecte sensores reais
5. ✅ Expanda funcionalidades

---

**Bora automatizar! 🚗💡**

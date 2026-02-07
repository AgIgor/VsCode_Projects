# 🆕 Novas Funcionalidades - v1.1

## ✨ O que há de novo

### 1. **Suporte a "SENAO" (Else)**

Agora você pode definir uma ação alternativa quando a condição não for verdadeira!

#### Sintaxe:
```
[tipo] [condição] entao [ação] senao [ação alternativa]
```

#### Exemplos:

**Controle de Porta - Liga/Desliga Automático:**
```
se porta == low entao luz-interna == high senao luz-interna == low
```
✅ Porta aberta (LOW) → Luz acende
✅ Porta fechada (HIGH) → Luz apaga

**Controle de Ignição:**
```
se ignicao == high entao farol == high em 2s senao farol == low
```
✅ Ignição ligada → Farol acende após 2s
✅ Ignição desligada → Farol apaga imediatamente

**Com Timers:**
```
se destrava == low entao luz-assoalho == high por 30s senao luz-assoalho == low
```
✅ Ao destravar → Luz assoalho por 30s
✅ Após 30s ou ao travar → Luz apaga

### 2. **Saídas como Condições**

Agora você pode ler o estado atual das saídas e usar como condição!

#### Sintaxe:
```
se [saida] == [estado] entao [ação]
```

#### Exemplos:

**Evitar Farol e DRL Simultâneos:**
```
se ignicao == high and farol == low entao drl == high
se farol == high entao drl == low
```

**Lógica Sequencial:**
```
se luz-interna == high entao luz-assoalho == high em 5s
```

**Interlocking (Travamento Mútuo):**
```
se drl == high entao farol == low
se farol == high entao drl == low
```

**Condições Complexas:**
```
se porta == low and luz-interna == low entao luz-assoalho == high
```

### 3. **Controle Manual de Saídas**

Agora você pode controlar manualmente cada saída diretamente da interface!

#### Como Usar:

1. **Clique no botão ON/OFF** ao lado de cada saída
2. O botão alterna entre:
   - 🟢 **OFF** → Saída está HIGH (ligada)
   - ⚪ **ON** → Saída está LOW (desligada)
3. O estado é sincronizado em tempo real com o ESP32
4. As regras continuam funcionando simultaneamente!

#### Recursos:
- ✅ Controle instantâneo
- ✅ Sincronização bidirecional (interface ↔ ESP32)
- ✅ Funciona junto com as regras
- ✅ Feedback visual imediato

## 📚 Exemplos Práticos Avançados

### Cenário 1: Sistema Inteligente de Cortesia

```
// Luz interna com porta e ignição
se porta == low and ignicao == low entao luz-interna == high por 30s senao luz-interna == low

// Luz assoalho apenas se luz interna está ligada
se luz-interna == high entao luz-assoalho == high senao luz-assoalho == low
```

**Comportamento:**
- Porta aberta + ignição desligada → Luz interna por 30s → Luz assoalho também acende
- Qualquer outra situação → Ambas apagam

### Cenário 2: Sistema de Faróis Automático

```
// DRL padrão com ignição
se ignicao == high and farol == low entao drl == high senao drl == low

// Farol manual (via botão ou regra)
// Quando farol liga, DRL apaga automaticamente

// Ao desligar ignição, ambos apagam
se ignicao == low entao farol == low
se ignicao == low entao drl == low
```

**Comportamento:**
- Ignição ligada → DRL acende
- Farol liga (manual ou automático) → DRL apaga
- Farol apaga → DRL volta
- Ignição desliga → Tudo apaga

### Cenário 3: Temporizadores Inteligentes

```
// Cortesia ao destravar
se destrava == low entao luz-interna == high por 15s senao luz-interna == low

// Se abrir porta durante cortesia, mantém ligado
se porta == low and luz-interna == high entao luz-interna == high

// Ao fechar porta, aguarda 10s
se porta == high entao luz-interna == low em 10s
```

**Comportamento:**
- Destravar → Luz por 15s
- Se abrir porta antes de 15s → Luz continua
- Fechar porta → Aguarda 10s e apaga

### Cenário 4: Interlocking de Saídas

```
// Apenas uma luz por vez (economia de bateria)
se luz-interna == high entao luz-assoalho == low
se luz-assoalho == high entao luz-interna == low

// Farol tem prioridade sobre DRL
se farol == high entao drl == low
```

**Comportamento:**
- Luz interna liga → Luz assoalho apaga
- Luz assoalho liga → Luz interna apaga
- Farol liga → DRL apaga (prioridade)

### Cenário 5: Sistema Completo v2

```
// === ENTRADAS ===

// Cortesia porta
se porta == low and ignicao == low entao luz-interna == high senao luz-interna == low

// Luz assoalho segue luz interna
se luz-interna == high entao luz-assoalho == high em 2s senao luz-assoalho == low

// === IGNIÇÃO ===

// DRL automático
se ignicao == high and farol == low entao drl == high senao drl == low

// Farol com delay
se ignicao == high entao farol == high em 5s senao farol == low

// === ALARME ===

// Piscadas ao travar
quando trava == low entao drl == high por 2s

// Piscadas ao destravar
quando destrava == low and ignicao == low entao farol == high por 2s

// === CORTESIA DESTRAVA ===

// Luz temporizada ao destravar
se destrava == low and porta == high entao luz-interna == high por 20s
```

## 🎯 Dicas de Uso

### Usando "SENAO" Eficientemente

✅ **Bom Uso:**
```
se porta == low entao luz-interna == high senao luz-interna == low
```
→ Controle direto: porta controla a luz

❌ **Uso Redundante:**
```
se porta == low entao luz-interna == high
se porta == high entao luz-interna == low
```
→ Duas regras para o mesmo efeito

### Leitura de Saídas

✅ **Bom Uso:**
```
se farol == high entao drl == low
```
→ Previne conflito automático

✅ **Uso Avançado:**
```
se luz-interna == high and porta == high entao luz-interna == low em 10s
```
→ Timer condicional baseado em estado

### Controle Manual + Regras

- Use controle manual para **testar** antes de criar regras
- Regras podem **sobrescrever** controle manual
- Se conflitar, a **última ação** prevalece
- Use "senao" para **reverter** ações manuais

## 📊 Saídas Disponíveis como Condições

| Saída | Nome no Código | Pode usar como condição? |
|-------|----------------|--------------------------|
| Luz Interna | `luz-interna` | ✅ Sim |
| Luz Assoalho | `luz-assoalho` | ✅ Sim |
| Farol | `farol` | ✅ Sim |
| DRL | `drl` | ✅ Sim |

## 🔄 Sincronização em Tempo Real

Todo o sistema funciona sincronizado:

1. **Controle Manual** → Atualiza interface → Envia ao ESP32 → Aplica regras
2. **Regras Automáticas** → ESP32 executa → Atualiza interface
3. **WebSocket** mantém tudo sincronizado em < 100ms

## 🐛 Resolução de Problemas

### "Senao" não funciona
- ✅ Verifique espaços: ` senao ` (com espaços)
- ✅ Use minúsculas: `senao` não `SENAO`
- ✅ Formato: `entao [ação] senao [ação]`

### Saída não aceita como condição
- ✅ Use nome correto: `luz-interna` não `luz interna`
- ✅ Formato: `luz-interna == high`
- ✅ Suportadas: `luz-interna`, `luz-assoalho`, `farol`, `drl`

### Controle manual não funciona
- ✅ Verifique WebSocket conectado (console)
- ✅ Recarregue a página
- ✅ Verifique se regras não estão sobrescrevendo

### Conflito entre regras e controle manual
- ❓ Última ação sempre prevalece
- 💡 Use `senao` para controle automático completo
- 💡 Desative regras conflitantes temporariamente

## 📖 Sintaxe Completa Atualizada

```
[tipo] [condição1] [operador] [condição2] entao [ação] [senao [ação]]

Tipos: se, quando, enquanto
Condições: entrada|saida == low|high
Operadores: and, or, e, ou
Ações: saida == low|high [em Xs] [por Ys]
```

### Exemplos de Cada Parte:

**Condição com entrada:**
```
porta == low
```

**Condição com saída:**
```
luz-interna == high
```

**Condição mista:**
```
porta == low and luz-interna == low
```

**Ação simples:**
```
luz-interna == high
```

**Ação com delay:**
```
farol == high em 2s
```

**Ação com duração:**
```
drl == high por 5s
```

**Ação com delay e duração:**
```
luz-assoalho == high em 2s por 30s
```

**Regra completa:**
```
se porta == low and luz-interna == low entao luz-interna == high por 30s senao luz-interna == low
```

## 🎓 Tutorial Rápido

### Passo 1: Teste Manual
1. Abra a interface
2. Clique nos botões ON/OFF das saídas
3. Veja o efeito imediato

### Passo 2: Primeira Regra com "Senao"
```
se porta == low entao luz-interna == high senao luz-interna == low
```

### Passo 3: Use Saída como Condição
```
se luz-interna == high entao luz-assoalho == high senao luz-assoalho == low
```

### Passo 4: Combine Tudo
```
se porta == low and ignicao == low entao luz-interna == high por 20s senao luz-interna == low
se luz-interna == high entao luz-assoalho == high em 1s senao luz-assoalho == low
```

---

**Versão:** 1.1.0  
**Data:** Fevereiro 2026  
**Novas Features:** SENAO, Saídas como Condições, Controle Manual

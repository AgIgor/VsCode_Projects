# Sistema Escalável de Controle de Botões para Arduino

Um sistema completo, reutilizável e escalável para controlar botões, temporizadores e saídas em Arduino.

## 🎯 Principais Características

✅ **Detecção de Mudança de Estado** - Detecta rising/falling edges com debounce automático  
✅ **Temporização Escalável** - Controle de-delay (on-delay, off-delay)  
✅ **Lógica AND/OR** - Combine múltiplas entradas  
✅ **Prioridade Entre Entradas** - Uma entrada tem prioridade sobre outra  
✅ **Timer Escalável** - Contador de tempo com callbacks  
✅ **Sistema de Latch** - Mantém saída ligada até comando  
✅ **Totalmente Modular** - Use apenas o que precisa  

## 📁 Estrutura do Projeto

```
src/
  main.cpp              ← Implementação com exemplos básicos
include/
  ButtonStateManager.h  ← Definições de classes
lib/
  ButtonStateManager.cpp ← Implementação das classes
DOCUMENTACAO.md         ← Documentação completa
EXEMPLOS_AVANCADOS.cpp ← 12 exemplos prontos para copiar
```

## 🚀 Como Usar

### 1. Incluir o Header
```cpp
#include "ButtonStateManager.h"
```

### 2. Criar Instâncias
```cpp
// Botão com detecção de borda de descida
Button botao(2, HIGH, 20, FALLING);

// Saída com delay: liga em 1s, desliga em 2s
OutputTimer saida(6, 1000, 2000);

// Lógica AND com 2 entradas
LogicGate logica(2, AND_MODE);

// Timer de 5 segundos
ScalableTimer timer(5000);
```

### 3. No Loop Principal
```cpp
void loop() {
  botao.update();           // Sempre atualizar!
  
  saida.trigger(botao.getState() == LOW);
  saida.update();
  
  timer.update();
  
  delay(10);
}
```

## 📚 Classes Disponíveis

| Classe | Responsabilidade | Escalável |
|--------|------------------|-----------|
| `Button` | Detecção de pressionamento com debounce | ✅ Múltiplos botões |
| `OutputTimer` | Temporização de saídas (on/off delay) | ✅ Múltiplas saídas |
| `LogicGate` | Lógica AND/OR entre entradas | ✅ N entradas |
| `PriorityOutput` | Uma saída controlada por 2 entradas com prioridade | ✅ Múltiplas saídas |
| `ScalableTimer` | Timer reutilizável com callbacks | ✅ Múltiplos timers |

## 💡 Exemplos Rápidos

### Exemplo 1: Botão liga LED
```cpp
Button btn(2, HIGH, 20, FALLING);
OutputTimer led(6, 0, 0);  // Sem delay

btn.update();
led.trigger(btn.getState() == LOW);
led.update();
digitalWrite(13, led.getOutputState());
```

### Exemplo 2: Duas entradas com AND
```cpp
LogicGate logica(2, AND_MODE);
logica.setInput(0, btn1.getState() == LOW);
logica.setInput(1, btn2.getState() == LOW);

if (logica.evaluate()) {
  digitalWrite(OUTPUT, HIGH);  // Liga só se AMBOS pressionados
}
```

### Exemplo 3: Timer com callback
```cpp
ScalableTimer timer(3000);
timer.onTimerComplete([]() {
  Serial.println("3 segundos!");
});

timer.start();
timer.update();
```

## 📖 Documentação Completa

Veja `DOCUMENTACAO.md` para:
- Descrição detalhada de cada classe
- Parâmetros e funções
- Exemplos práticos
- Dicas de implementação

## 🎓 Exemplos Avançados

Veja `EXEMPLOS_AVANCADOS.cpp` para 12 exemplos completos:
1. Detecção de mudança de estado
2. Temporização 4 saídas com AND/OR
3. Saída escalável
4. Sistema de latch
5. Uma entrada temporizada
6. Prioridade entre entradas
7. Contador escalável
8. Botão com contagem
9. Delay OFF ao soltar
10. Rising/Falling edges
11. Sistema completo integrado
12. Máquina de estados com timers

## 🔧 Compilação e Upload

Com PlatformIO já configurado:
```bash
pio run --target upload
```

## 💾 Uso em Seu Projeto

1. Copie `include/ButtonStateManager.h`
2. Copie `lib/ButtonStateManager.cpp`
3. Inclua em seus arquivos: `#include "ButtonStateManager.h"`
4. Crie instâncias das classes
5. Chame `update()` no loop principal

## 📝 Notas Importantes

- ⚠️ **Sempre chamar `.update()`** nos objetos no loop
- 🔌 Botões com PULLUP: `getState() == LOW` significa pressionado
- ⏱️ Delays em milisegundos (1000 = 1 segundo)
- 🔄 Crie múltiplas instâncias para múltiplas saídas
- ⚙️ Alle configurações podem ser mudadas em runtime

## 📞 Suporte

Para questões ou melhorias, consulte a documentação ou revise os exemplos.

---

**Versão:** 1.0  
**Compatibilidade:** Arduino (ATmega), PlatformIO  
**Linguagem:** C++

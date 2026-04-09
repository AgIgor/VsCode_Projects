#!/usr/bin/env python3
"""
Controle remoto para Fonte de Bancada Arduino PID via Serial
Requer: pip install pyserial

Uso:
    python3 psu_control.py COM3          # Windows
    python3 psu_control.py /dev/ttyUSB0 # Linux
"""

import sys
import serial
import time
import threading
from datetime import datetime

class PSUController:
    def __init__(self, port, baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=1)
            time.sleep(2)  # Aguardar inicialização do Arduino
            print(f"[OK] Conectado em {port} @ {baudrate} bps")
        except Exception as e:
            print(f"[ERRO] Não foi possível conectar: {e}")
            sys.exit(1)
        
        self.running = True
        self.monitor_thread = None
    
    def send_command(self, cmd):
        """Enviar comando e aguardar resposta"""
        try:
            self.ser.write(f"{cmd}\n".encode())
            self.ser.flush()
        except Exception as e:
            print(f"[ERRO] Falha ao enviar: {e}")
    
    def read_response(self, timeout=1):
        """Ler resposta do Arduino"""
        try:
            start = time.time()
            while time.time() - start < timeout:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode().strip()
                    if line:
                        return line
            return None
        except Exception as e:
            print(f"[ERRO] Falha ao ler: {e}")
            return None
    
    def send_and_wait(self, cmd, timeout=2):
        """Enviar e aguardar resposta"""
        self.send_command(cmd)
        return self.read_response(timeout)
    
    def start_monitor(self):
        """Iniciar monitoramento contínuo em thread"""
        self.monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.monitor_thread.start()
        print("[OK] Monitoramento iniciado")
    
    def _monitor_loop(self):
        """Loop de monitoramento"""
        while self.running:
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode().strip()
                    if line:
                        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        print(f"[{timestamp}] {line}")
            except Exception as e:
                if self.running:
                    print(f"[ERRO] Monitor: {e}")
            time.sleep(0.01)
    
    def stop_monitor(self):
        """Parar monitoramento"""
        self.running = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=1)
    
    def enable(self):
        """Ativar saída"""
        response = self.send_and_wait("E1")
        print(f"Enable: {response}")
    
    def disable(self):
        """Desativar saída"""
        response = self.send_and_wait("E0")
        print(f"Disable: {response}")
    
    def set_voltage(self, voltage):
        """Definir tensão alvo (0-24V)"""
        response = self.send_and_wait(f"V{voltage:.2f}")
        print(f"Set Voltage: {response}")
    
    def set_current(self, current):
        """Definir corrente alvo (0-3A)"""
        response = self.send_and_wait(f"C{current:.2f}")
        print(f"Set Current: {response}")
    
    def set_kp(self, kp):
        """Definir ganho proporcional"""
        response = self.send_and_wait(f"P{kp:.4f}")
        print(f"Set Kp: {response}")
    
    def set_ki(self, ki):
        """Definir ganho integral"""
        response = self.send_and_wait(f"I{ki:.4f}")
        print(f"Set Ki: {response}")
    
    def set_kd(self, kd):
        """Definir ganho derivativo"""
        response = self.send_and_wait(f"D{kd:.4f}")
        print(f"Set Kd: {response}")
    
    def start_tuning(self):
        """Iniciar auto-tuning"""
        print("[INFO] Iniciando auto-tuning (aguarde ~30 segundos)...")
        self.send_command("T")
        
        # Aguardar conclusão do tuning
        tuning_complete = False
        start_time = time.time()
        timeout = 60  # 60 segundos máximo
        
        while not tuning_complete and (time.time() - start_time) < timeout:
            line = self.read_response(timeout=2)
            if line and "TUNE:COMPLETE" in line:
                print(f"[OK] {line}")
                tuning_complete = True
            elif line:
                print(f"     {line}")
            time.sleep(0.1)
        
        if not tuning_complete:
            print("[AVISO] Auto-tuning expirou ou foi abortado")
    
    def get_status(self):
        """Obter status atual"""
        response = self.send_and_wait("G")
        if response:
            print(f"Status: {response}")
            return response
        return None
    
    def help(self):
        """Exibir ajuda"""
        response = self.send_and_wait("H")
        time.sleep(0.5)  # Dar tempo para receber todas as linhas
    
    def close(self):
        """Fechar conexão"""
        self.stop_monitor()
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[OK] Conexão fechada")


def interactive_menu(psu):
    """Menu interativo"""
    print("\n=== PSU Control Menu ===")
    print("1. Set Voltage")
    print("2. Set Current")
    print("3. Enable Output")
    print("4. Disable Output")
    print("5. Set Kp")
    print("6. Set Ki")
    print("7. Set Kd")
    print("8. Start Auto-Tuning")
    print("9. Get Status")
    print("0. Exit")
    
    while True:
        try:
            choice = input("\nChoice: ").strip()
            
            if choice == '1':
                voltage = float(input("Voltage (0-24V): "))
                psu.set_voltage(voltage)
            
            elif choice == '2':
                current = float(input("Current (0-3A): "))
                psu.set_current(current)
            
            elif choice == '3':
                psu.enable()
            
            elif choice == '4':
                psu.disable()
            
            elif choice == '5':
                kp = float(input("Kp value: "))
                psu.set_kp(kp)
            
            elif choice == '6':
                ki = float(input("Ki value: "))
                psu.set_ki(ki)
            
            elif choice == '7':
                kd = float(input("Kd value: "))
                psu.set_kd(kd)
            
            elif choice == '8':
                psu.start_tuning()
            
            elif choice == '9':
                psu.get_status()
            
            elif choice == '0':
                break
        
        except ValueError:
            print("[ERRO] Valor inválido")
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"[ERRO] {e}")


def demo_mode(psu):
    """Modo demonstração"""
    print("\n=== Modo Demonstração ===\n")
    
    # Desativar
    print("1. Desativar saída")
    psu.disable()
    time.sleep(1)
    
    # Ativar com 5V
    print("\n2. Ativar saída com 5V")
    psu.enable()
    psu.set_voltage(5.0)
    psu.set_current(1.0)
    time.sleep(3)
    psu.get_status()
    
    # Aumentar para 12V
    print("\n3. Aumentar para 12V")
    psu.set_voltage(12.0)
    time.sleep(3)
    psu.get_status()
    
    # Auto-tuning
    print("\n4. Executar auto-tuning")
    psu.start_tuning()
    time.sleep(2)
    psu.get_status()
    
    # Desativar
    print("\n5. Desativar")
    psu.disable()
    print("\n[OK] Demonstração concluída")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 psu_control.py <PORTA> [demo]")
        print("Exemplo:")
        print("  python3 psu_control.py COM3       # Menu interativo")
        print("  python3 psu_control.py COM3 demo  # Modo demonstração")
        sys.exit(1)
    
    port = sys.argv[1]
    psu = PSUController(port)
    
    try:
        psu.start_monitor()
        time.sleep(1)
        
        if len(sys.argv) > 2 and sys.argv[2] == "demo":
            demo_mode(psu)
        else:
            interactive_menu(psu)
    
    except KeyboardInterrupt:
        print("\n[INFO] Interrompido pelo usuário")
    
    finally:
        psu.close()

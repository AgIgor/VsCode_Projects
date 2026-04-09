@echo off
REM RealDash ESP32 Sender - Setup and Run
REM Windows Batch Script

echo.
echo ====================================
echo RealDash ESP32 Sender - Instalador
echo ====================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo Erro: Python nao encontrado!
    echo Por favor, instale Python 3.10+ de https://python.org
    pause
    exit /b 1
)

echo [1/2] Instalando dependencias...
python -m pip install -q -r requirements.txt

if errorlevel 1 (
    echo Erro ao instalar dependencias!
    pause
    exit /b 1
)

echo [2/2] Iniciando aplicativo...
python main.py

if errorlevel 1 (
    echo Erro ao iniciar aplicativo
    pause
    exit /b 1
)

@echo off
cd /d "%~dp0python-realdash-sender"
python -m pip install -r requirements.txt
python main.py

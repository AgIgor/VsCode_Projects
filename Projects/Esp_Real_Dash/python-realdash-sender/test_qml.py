#!/usr/bin/env python
"""Quick test to validate QML syntax"""
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine

def test_qml():
    app = QGuiApplication(sys.argv)
    engine = QQmlApplicationEngine()
    
    qml_file = os.path.join(os.path.dirname(__file__), "qml", "Main.qml")
    engine.load(os.path.abspath(qml_file))
    
    if not engine.rootObjects():
        print("ERROR: QML failed to load")
        return False
    
    print("✅ QML loaded successfully!")
    return True

if __name__ == "__main__":
    success = test_qml()
    sys.exit(0 if success else 1)

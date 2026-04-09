import os
import sys

from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine

from telemetry_sender import TelemetrySender


def main() -> int:
    app = QGuiApplication(sys.argv)
    engine = QQmlApplicationEngine()

    sender = TelemetrySender()
    engine.rootContext().setContextProperty("telemetrySender", sender)

    qml_file = os.path.join(os.path.dirname(__file__), "qml", "Main.qml")
    engine.load(os.path.abspath(qml_file))

    if not engine.rootObjects():
        return -1

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

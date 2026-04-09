#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "telemetrysender.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    TelemetrySender sender;
    engine.rootContext()->setContextProperty("telemetrySender", &sender);

    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}

#include "telemetrysender.h"

#include <QTextStream>
#include <QtGlobal>

TelemetrySender::TelemetrySender(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &TelemetrySender::sendNow);
    m_timer.setInterval(m_intervalMs);
    m_timer.start();

    connect(&m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) {
            return;
        }
        emit serialError(m_serial.errorString());
    });

    refreshPorts();
}

QStringList TelemetrySender::availablePorts() const { return m_availablePorts; }
QString TelemetrySender::portName() const { return m_portName; }
int TelemetrySender::baudRate() const { return m_baudRate; }
int TelemetrySender::intervalMs() const { return m_intervalMs; }
bool TelemetrySender::connected() const { return m_serial.isOpen(); }

double TelemetrySender::rpm() const { return m_rpm; }
double TelemetrySender::speed() const { return m_speed; }
double TelemetrySender::coolant() const { return m_coolant; }
double TelemetrySender::throttle() const { return m_throttle; }
double TelemetrySender::map() const { return m_map; }
double TelemetrySender::voltage() const { return m_voltage; }
double TelemetrySender::fuel() const { return m_fuel; }

void TelemetrySender::setPortName(const QString &portName)
{
    if (m_portName == portName)
        return;
    m_portName = portName;
    emit portNameChanged();
}

void TelemetrySender::setBaudRate(int baudRate)
{
    if (m_baudRate == baudRate)
        return;
    m_baudRate = baudRate;
    emit baudRateChanged();
}

void TelemetrySender::setIntervalMs(int intervalMs)
{
    if (intervalMs < 20)
        intervalMs = 20;
    if (intervalMs > 2000)
        intervalMs = 2000;
    if (m_intervalMs == intervalMs)
        return;
    m_intervalMs = intervalMs;
    m_timer.setInterval(m_intervalMs);
    emit intervalMsChanged();
}

void TelemetrySender::setRpm(double value)
{
    value = qBound(0.0, value, 12000.0);
    if (qFuzzyCompare(m_rpm, value))
        return;
    m_rpm = value;
    emit telemetryChanged();
}

void TelemetrySender::setSpeed(double value)
{
    value = qBound(0.0, value, 320.0);
    if (qFuzzyCompare(m_speed, value))
        return;
    m_speed = value;
    emit telemetryChanged();
}

void TelemetrySender::setCoolant(double value)
{
    value = qBound(-40.0, value, 215.0);
    if (qFuzzyCompare(m_coolant, value))
        return;
    m_coolant = value;
    emit telemetryChanged();
}

void TelemetrySender::setThrottle(double value)
{
    value = qBound(0.0, value, 100.0);
    if (qFuzzyCompare(m_throttle, value))
        return;
    m_throttle = value;
    emit telemetryChanged();
}

void TelemetrySender::setMap(double value)
{
    value = qBound(0.0, value, 255.0);
    if (qFuzzyCompare(m_map, value))
        return;
    m_map = value;
    emit telemetryChanged();
}

void TelemetrySender::setVoltage(double value)
{
    value = qBound(0.0, value, 65.0);
    if (qFuzzyCompare(m_voltage, value))
        return;
    m_voltage = value;
    emit telemetryChanged();
}

void TelemetrySender::setFuel(double value)
{
    value = qBound(0.0, value, 100.0);
    if (qFuzzyCompare(m_fuel, value))
        return;
    m_fuel = value;
    emit telemetryChanged();
}

void TelemetrySender::refreshPorts()
{
    QStringList newPorts;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        newPorts << port.portName();
    }

    if (m_availablePorts != newPorts) {
        m_availablePorts = newPorts;
        emit availablePortsChanged();
    }

    if (m_portName.isEmpty() && !m_availablePorts.isEmpty()) {
        m_portName = m_availablePorts.first();
        emit portNameChanged();
    }
}

bool TelemetrySender::connectSerial()
{
    if (m_serial.isOpen()) {
        return true;
    }

    if (m_portName.isEmpty()) {
        emit serialError(QStringLiteral("Selecione uma porta COM"));
        return false;
    }

    m_serial.setPortName(m_portName);
    m_serial.setBaudRate(m_baudRate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    const bool opened = m_serial.open(QIODevice::ReadWrite);
    if (!opened) {
        emit serialError(m_serial.errorString());
        return false;
    }

    emit connectedChanged();
    return true;
}

void TelemetrySender::disconnectSerial()
{
    if (!m_serial.isOpen()) {
        return;
    }

    m_serial.close();
    emit connectedChanged();
}

QString TelemetrySender::buildPayload() const
{
    QString payload;
    QTextStream stream(&payload);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(1);

    stream << "RPM=" << m_rpm
           << ",SPEED=" << m_speed
           << ",ECT=" << m_coolant
           << ",TPS=" << m_throttle
           << ",MAP=" << m_map
           << ",VBAT=";

    stream.setRealNumberPrecision(2);
    stream << m_voltage;

    stream.setRealNumberPrecision(1);
    stream << ",FUEL=" << m_fuel << "\n";

    return payload;
}

void TelemetrySender::sendNow()
{
    if (!m_serial.isOpen()) {
        return;
    }

    const QString payload = buildPayload();
    const qint64 written = m_serial.write(payload.toUtf8());
    if (written < 0) {
        emit serialError(m_serial.errorString());
    }
}

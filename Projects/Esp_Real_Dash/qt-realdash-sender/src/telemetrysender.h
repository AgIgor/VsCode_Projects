#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QStringList>

class TelemetrySender : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

    Q_PROPERTY(double rpm READ rpm WRITE setRpm NOTIFY telemetryChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY telemetryChanged)
    Q_PROPERTY(double coolant READ coolant WRITE setCoolant NOTIFY telemetryChanged)
    Q_PROPERTY(double throttle READ throttle WRITE setThrottle NOTIFY telemetryChanged)
    Q_PROPERTY(double map READ map WRITE setMap NOTIFY telemetryChanged)
    Q_PROPERTY(double voltage READ voltage WRITE setVoltage NOTIFY telemetryChanged)
    Q_PROPERTY(double fuel READ fuel WRITE setFuel NOTIFY telemetryChanged)

public:
    explicit TelemetrySender(QObject *parent = nullptr);

    QStringList availablePorts() const;
    QString portName() const;
    int baudRate() const;
    int intervalMs() const;
    bool connected() const;

    double rpm() const;
    double speed() const;
    double coolant() const;
    double throttle() const;
    double map() const;
    double voltage() const;
    double fuel() const;

    void setPortName(const QString &portName);
    void setBaudRate(int baudRate);
    void setIntervalMs(int intervalMs);

    void setRpm(double value);
    void setSpeed(double value);
    void setCoolant(double value);
    void setThrottle(double value);
    void setMap(double value);
    void setVoltage(double value);
    void setFuel(double value);

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE bool connectSerial();
    Q_INVOKABLE void disconnectSerial();
    Q_INVOKABLE void sendNow();

signals:
    void availablePortsChanged();
    void portNameChanged();
    void baudRateChanged();
    void intervalMsChanged();
    void connectedChanged();
    void telemetryChanged();
    void serialError(const QString &message);

private:
    QString buildPayload() const;

private:
    QSerialPort m_serial;
    QTimer m_timer;
    QStringList m_availablePorts;
    QString m_portName;
    int m_baudRate = 115200;
    int m_intervalMs = 100;

    double m_rpm = 900.0;
    double m_speed = 0.0;
    double m_coolant = 80.0;
    double m_throttle = 0.0;
    double m_map = 35.0;
    double m_voltage = 13.8;
    double m_fuel = 75.0;
};

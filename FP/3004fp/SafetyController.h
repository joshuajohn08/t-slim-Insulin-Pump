#ifndef SAFETYCONTROLLER_H
#define SAFETYCONTROLLER_H

#include <QObject>
#include <QTimer>

// Manages safety-related monitoring such as battery and insulin levels
class SafetyController : public QObject
{
    Q_OBJECT

public:
    explicit SafetyController(QObject *parent = nullptr);

signals:
    // Emitted when battery level changes
    void batteryLevelUpdated(int level);

    // Emitted when battery is critically low
    void triggerBatteryAlert();

    // Emitted when battery is fully depleted
    void batteryDepleted();

    // Emitted when insulin level changes
    void insulinLevelUpdated(int level);

    // Emitted when insulin is critically low
    void triggerLowInsulinAlert();

public slots:
    // Decrease battery level periodically
    void decreaseBattery();

    // Recharge battery to full
    void rechargeBattery();

    // Automatically reduce insulin over time
    void decreaseInsulin();

    // Update insulin level on manual delivery
    void registerInsulinDelivery(int);

private:
    int batteryLevel;                 // Current battery percentage
    QTimer batteryTimer;             // Timer to simulate battery drain
    bool lowBatteryWarned;           // Tracks if low battery warning has been issued

    int insulinLevel = 100;          // Initial insulin units
    bool lowInsulinWarned = false;   // Tracks if low insulin warning has been issued
};

#endif // SAFETYCONTROLLER_H

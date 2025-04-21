#ifndef SAFETYCONTROLLER_H
#define SAFETYCONTROLLER_H

#include <QObject>
#include <QTimer>

// Manages safety-related monitoring such as battery and insulin levels
class SafetyController : public QObject
{
    Q_OBJECT

public:
    // Constructor initializes safety controller with parent context
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

    // Refill insulin
    void refillInsulin();

    // Automatically reduce insulin over time
    void decreaseInsulin();

    // Update insulin level on manual delivery
    void registerInsulinDelivery(double);

    // Set a new basal rate (u/h) for insulin delivery
    void setBasalRate(double rate);

    // Adjust the current basal rate by a specified amount
    void adjustBasalRate(double adjustment);

    // Retrieve the current basal rate (u/h)
    double getBasalRate() const;

private:
    int batteryLevel;                 // Current battery percentage
    QTimer batteryTimer;             // Timer to simulate battery drain
    bool lowBatteryWarned;           // Tracks if low battery warning has been issued

    int insulinLevel = 100;          // Initial insulin units
    bool lowInsulinWarned = false;   // Tracks if low insulin warning has been issued
    double currentBasalRate = 1.0;   // Default rate in u/h
};

#endif // SAFETYCONTROLLER_H

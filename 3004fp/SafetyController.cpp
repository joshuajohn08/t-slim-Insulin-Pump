#include "SafetyController.h"
#include <QString>
#include <QDebug>

// Constructor initializes battery level and starts timer for simulating battery drain
SafetyController::SafetyController(QObject *parent)
    : QObject(parent), batteryLevel(100), lowBatteryWarned(false)
{
    connect(&batteryTimer, &QTimer::timeout, this, &SafetyController::decreaseBattery);
    batteryTimer.start(1000); // Simulate fast battery depletion (1 second interval)
}

// Called every second by QTimer to simulate battery usage
void SafetyController::decreaseBattery()
{
    if (batteryLevel > 0) {
        batteryLevel -= 2;
        emit batteryLevelUpdated(batteryLevel); // Notify UI or system of new battery level

        // Trigger alert once when battery drops below or equals 20%
        if (batteryLevel <= 20 && !lowBatteryWarned) {
            emit triggerBatteryAlert();
            lowBatteryWarned = true;
        }

        // If battery reaches 0, stop timer and signal shutdown
        if (batteryLevel <= 0) {
            batteryTimer.stop();
            emit batteryDepleted(); // Could trigger pump suspension or UI error
        }
    }
}

// Fully recharge the battery and reset alert flag
void SafetyController::rechargeBattery()
{
    batteryLevel = 100;
    emit batteryLevelUpdated(batteryLevel);
    lowBatteryWarned = false;
}

// Global insulin variables simulate shared device state
int insulinLevel = 100;        // Initial insulin reservoir (units)
bool lowInsulinWarned = false;

// Simulates gradual insulin depletion from basal or bolus delivery
void SafetyController::decreaseInsulin() {
    if (insulinLevel > 0) {
        insulinLevel -= 1;
        emit insulinLevelUpdated(insulinLevel); // Notify system/UI

        // Alert user if insulin falls below or equals 50 units
        if (insulinLevel <= 50 && !lowInsulinWarned) {
            emit triggerLowInsulinAlert();
            lowInsulinWarned = true;
        }
    }
}

// Called when insulin is added to the reservoir (e.g., refill)
void SafetyController::registerInsulinDelivery(double amount) {
    if (amount > 0) return;
    insulinLevel += amount;
    if (insulinLevel < 0) insulinLevel = 0;
    emit insulinLevelUpdated(insulinLevel);

    if (insulinLevel <= 20 && !lowInsulinWarned) {
        emit triggerLowInsulinAlert();
        lowInsulinWarned = true;
    }
}



void SafetyController::setBasalRate(double rate) {
    currentBasalRate = rate;
    qDebug() << "[SafetyController] Basal rate set to" << rate << "u/h";
}

void SafetyController::adjustBasalRate(double adjustment) {
    currentBasalRate += adjustment;
    if (currentBasalRate < 0.05) currentBasalRate = 0.05;
    if (currentBasalRate > 5.0) currentBasalRate = 5.0;
    qDebug() << "[SafetyController] Basal rate adjusted by" << adjustment << "->" << currentBasalRate;
}

double SafetyController::getBasalRate() const {
    return currentBasalRate;
}

void SafetyController::refillInsulin() {
    insulinLevel = 200;
    emit insulinLevelUpdated(insulinLevel);
    lowInsulinWarned = false;
}


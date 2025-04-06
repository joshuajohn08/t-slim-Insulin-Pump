#include "BolusManager.h"

BolusManager::BolusManager() {
    bolusInProgress = false;
    partialDelivered = 0.0;
}

// Returns the total bolus calculated in the last result
double BolusManager::getTotalBolus() const {
    return lastResult.totalBolus;
}

// Computes insulin bolus based on inputs and stores result
BolusResult BolusManager::calculateBolus(double carbs, double bg, double ICR, double CF, double targetBG, double IOB, double immediateFrac, int hours) {
    BolusResult result;
    result.carbBolus = carbs / ICR;
    result.correctionBolus = (bg - targetBG) / CF;
    result.totalBolus = result.carbBolus + result.correctionBolus;
    result.finalBolus = result.totalBolus - IOB;

    if (result.finalBolus < 0)
        result.finalBolus = 0;

    result.immediateBolus = immediateFrac * result.finalBolus;
    result.extendedBolus = result.finalBolus - result.immediateBolus;
    result.hourlyRate = (hours > 0) ? result.extendedBolus / hours : 0;

    lastResult = result;
    return result;
}

// Converts a BolusResult to a human-readable string summary
QString BolusResult::toString(int hours) const {
    QString log;
    log += QString("Carb Bolus: %1 units\n").arg(carbBolus, 0, 'f', 2);
    log += QString("Correction Bolus: %1 units\n").arg(correctionBolus, 0, 'f', 2);
    log += QString("Total (before IOB): %1 units\n").arg(totalBolus, 0, 'f', 2);
    log += QString("Final Bolus (after IOB): %1 units\n").arg(finalBolus, 0, 'f', 2);
    log += QString("Immediate: %1 units\n").arg(immediateBolus, 0, 'f', 2);
    log += QString("Extended: %1 units over %2 hrs (%3/hr)\n")
        .arg(extendedBolus, 0, 'f', 2).arg(hours).arg(hourlyRate, 0, 'f', 2);
    return log;
}

// Simulates bolus delivery and returns a delivery log
QString BolusManager::deliverBolus(const BolusResult& result, bool extended) {
    bolusInProgress = true;
    startTime = QDateTime::currentDateTime();
    partialDelivered = result.immediateBolus;

    QString log;
    log += "Bolus Delivery Started at " + startTime.toString("hh:mm:ss") + "\n";
    log += QString("Immediate dose delivered: %1 units\n").arg(result.immediateBolus, 0, 'f', 2);

    if (extended) {
        log += QString("Extended dose scheduled: %1 units over next hours.\n")
               .arg(result.extendedBolus, 0, 'f', 2);
    } else {
        partialDelivered += result.extendedBolus;
        log += QString("Full dose delivered immediately: %1 units\n").arg(result.finalBolus, 0, 'f', 2);
        bolusInProgress = false;
    }

    return log;
}

// Cancels an ongoing bolus and returns a cancellation log
QString BolusManager::cancelBolus() {
    if (!bolusInProgress) {
        return "No bolus is currently in progress.\n";
    }

    bolusInProgress = false;
    QDateTime cancelTime = QDateTime::currentDateTime();
    QString log;
    log += "Bolus delivery cancelled at " + cancelTime.toString("hh:mm:ss") + "\n";
    log += QString("Partial dose delivered: %1 units\n").arg(partialDelivered, 0, 'f', 2);
    return log;
}

// Updates insulin-on-board value after a dose is delivered
double BolusManager::updateIOB(double currentIOB, double deliveredAmount) {
    return currentIOB + deliveredAmount;
}

#ifndef BOLUSMANAGER_H
#define BOLUSMANAGER_H

#include <QString>
#include <QDateTime>

// Struct to store results of a bolus calculation
struct BolusResult {
    double carbBolus;        // Bolus for carbs
    double correctionBolus;  // Correction for high BG
    double totalBolus;       // Sum of carb and correction bolus
    double finalBolus;       // Final amount after subtracting IOB
    double immediateBolus;   // Portion to deliver immediately
    double extendedBolus;    // Portion to deliver over time
    double hourlyRate;       // Rate for extended delivery

    QString toString(int hours = 3) const;  // String summary of result
};

// Handles bolus calculation and delivery logic
class BolusManager {
public:
    BolusManager();

    double getTotalBolus() const;  // Returns last total bolus value

    // Calculates bolus based on inputs like carbs, BG, ICR, CF, etc.
    BolusResult calculateBolus(double carbs, double bg, double ICR, double CF, double targetBG, double IOB, double immediateFrac = 0.6, int hours = 3);

    // Simulates insulin delivery, optionally with extended delivery
    QString deliverBolus(const BolusResult& result, bool extended = false);

    // Cancels ongoing bolus and returns log message
    QString cancelBolus();

    // Updates IOB after delivery
    double updateIOB(double currentIOB, double deliveredAmount);

    // Returns the result of the last bolus calculation
    BolusResult getLastResult() const { return lastResult; }

private:
    bool bolusInProgress;      // Indicates if bolus is active
    double partialDelivered;   // Tracks how much was delivered
    QDateTime startTime;       // Timestamp for bolus start
    BolusResult lastResult;    // Stores last calculation result
};

#endif // BOLUSMANAGER_H

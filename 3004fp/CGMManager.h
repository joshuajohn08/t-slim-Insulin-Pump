#ifndef CGMMANAGER_H
#define CGMMANAGER_H

#include <QDateTime>
#include <QVector>
#include <QPair>
#include "BolusManager.h"

// Manages CGM data and insulin adjustment logic
class CGMManager {
public:
    CGMManager(BolusManager* bolusManager);

    // Structure to represent a CGM reading
    struct GlucoseReading {
        QDateTime timestamp;
        double value;   // Glucose level in mmol/L
        bool isAlarm;   // True if value triggers alarm
    };

    // Add a new CGM reading
    void addReading(double glucoseLevel);

    // Return glucose history for the last 'minutes' (default 60)
    QVector<QPair<double, double>> getGlucoseHistory(int minutes = 60) const;

    // Calculate insulin adjustment based on current CGM data
    double calculateInsulinAdjustment(double currentGlucose, double targetGlucose,
                                      double insulinSensitivity, double currentBasalRate);

    // Apply insulin adjustment based on CGM input (returns true if adjusted)
    bool adjustInsulinDelivery(double currentGlucose, double targetGlucose,
                               double insulinSensitivity, double currentBasalRate);

    // Predict glucose levels using current CGM/IOB/carb/basal info
    QVector<QPair<double, double>> predictGlucoseLevels(double currentGlucose,
                                                        double insulinOnBoard,
                                                        double carbsOnBoard,
                                                        double basalRate,
                                                        int timeSpanMinutes = 60);

    // Configure alert thresholds for high and low glucose
    void setAlerts(double lowGlucoseThreshold, double highGlucoseThreshold);

    // Check if current glucose value triggers an alert
    bool checkAlerts(double currentGlucose);

    // Return the latest CGM reading
    GlucoseReading getLatestReading() const;

private:
    BolusManager* m_bolusManager;          // Reference to bolus logic
    QVector<GlucoseReading> m_readings;    // List of recent CGM readings
    double m_lowGlucoseThreshold;          // Hypo alert threshold
    double m_highGlucoseThreshold;         // Hyper alert threshold
    double m_lastAdjustmentTime;           // Timestamp for last insulin adjustment

    // Calculate CGM glucose trend over time
    double calculateGlucoseRateOfChange(int minutesBack = 15) const;
};

#endif // CGMMANAGER_H

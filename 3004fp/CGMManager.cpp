#include "CGMManager.h"
#include <QDebug>

CGMManager::CGMManager(BolusManager* bolusManager) :
    m_bolusManager(bolusManager),
    m_lowGlucoseThreshold(3.9),  // Default 3.9 mmol/L (70 mg/dL)
    m_highGlucoseThreshold(10.0), // Default 10.0 mmol/L (180 mg/dL)
    m_lastAdjustmentTime(0)
{
}

void CGMManager::addReading(double glucoseLevel) {
    GlucoseReading reading;
    reading.timestamp = QDateTime::currentDateTime();
    reading.value = glucoseLevel;
    reading.isAlarm = checkAlerts(glucoseLevel);

    m_readings.append(reading);

    // Keep only recent readings (e.g., last 24 hours)
    while (m_readings.size() > 288) { // 288 readings = 24 hours @ 5 min intervals
        m_readings.removeFirst();
    }
}

QVector<QPair<double, double>> CGMManager::getGlucoseHistory(int minutes) const {
    QVector<QPair<double, double>> history;
    QDateTime currentTime = QDateTime::currentDateTime();
    QDateTime cutoffTime = currentTime.addSecs(-minutes * 60);

    for (const auto& reading : m_readings) {
        if (reading.timestamp >= cutoffTime) {
            // Time in minutes since start (x-axis), Glucose value (y-axis)
            double timeInMinutes = cutoffTime.secsTo(reading.timestamp) / 60.0;
            history.append(qMakePair(timeInMinutes, reading.value));
        }
    }

    return history;
}

double CGMManager::calculateGlucoseRateOfChange(int minutesBack) const {
    if (m_readings.size() < 2) {
        return 0.0; // Not enough data
    }

    QDateTime currentTime = QDateTime::currentDateTime();
    QDateTime cutoffTime = currentTime.addSecs(-minutesBack * 60);

    QVector<GlucoseReading> recentReadings;
    for (const auto& reading : m_readings) {
        if (reading.timestamp >= cutoffTime) {
            recentReadings.append(reading);
        }
    }

    if (recentReadings.size() < 2) {
        return 0.0;
    }

    // Calculate linear regression to find trend
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = recentReadings.size();

    for (int i = 0; i < n; i++) {
        double x = cutoffTime.secsTo(recentReadings[i].timestamp) / 60.0; // x in minutes
        double y = recentReadings[i].value;

        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    // Slope = (n*sumXY - sumX*sumY) / (n*sumX2 - sumX^2)
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);

    // Return rate of change in mmol/L per minute
    return slope;
}

double CGMManager::calculateInsulinAdjustment(double currentGlucose, double targetGlucose,
                                             double insulinSensitivity, double currentBasalRate) {
    // Calculate adjustment based on:
    // 1. Current glucose vs target
    // 2. Rate of change of glucose

    double glucoseDiff = currentGlucose - targetGlucose;
    double rateOfChange = calculateGlucoseRateOfChange();

    // Basic adjustment for current glucose level
    double adjustment = glucoseDiff / insulinSensitivity;

    // Additional adjustment for glucose trend
    // If glucose is rising quickly, add more insulin
    // If glucose is falling quickly, reduce insulin
    adjustment += (rateOfChange * 30) / insulinSensitivity; // Predict 30 min ahead

    // Limit adjustment to a percentage of basal rate
    double maxAdjustment = currentBasalRate * 0.5; // Max 50% adjustment

    if (adjustment > maxAdjustment) {
        adjustment = maxAdjustment;
    } else if (adjustment < -maxAdjustment) {
        adjustment = -maxAdjustment;
    }

    return adjustment;
}

bool CGMManager::adjustInsulinDelivery(double currentGlucose, double targetGlucose,
                                      double insulinSensitivity, double currentBasalRate) {
    // Check if enough time has passed since last adjustment (prevent too frequent changes)
    double currentTime = QDateTime::currentDateTime().toSecsSinceEpoch() / 60.0; // time in minutes
    if (currentTime - m_lastAdjustmentTime < 15) { // Only adjust every 15 min
        return false;
    }

    double adjustment = calculateInsulinAdjustment(currentGlucose, targetGlucose,
                                                 insulinSensitivity, currentBasalRate);

    // New basal rate with adjustment
    double newBasalRate = currentBasalRate + adjustment;

    // Safety checks - never go below minimum or above maximum
    if (newBasalRate < 0.05) {
        newBasalRate = 0.05; // Minimum basal rate
    } else if (newBasalRate > currentBasalRate * 2) {
        newBasalRate = currentBasalRate * 2; // Maximum double the programmed rate
    }

    // Relevant debug statements we decided to keep for testing
    qDebug() << "CGM adjusted insulin delivery: Current BG:" << currentGlucose
             << "Target:" << targetGlucose
             << "Adjustment:" << adjustment
             << "New Basal Rate:" << newBasalRate;

    // Update last adjustment time
    m_lastAdjustmentTime = currentTime;

    return true;
}

QVector<QPair<double, double>> CGMManager::predictGlucoseLevels(double currentGlucose,
                                                              double insulinOnBoard,
                                                              double carbsOnBoard,
                                                              double basalRate,
                                                              int timeSpanMinutes) {
    QVector<QPair<double, double>> predictions;

    // Simplified model parameters
    double insulinSensitivity = 2.0; // mmol/L drop per unit insulin
    double carbSensitivity = 0.2;    // mmol/L rise per gram carbs
    double basalImpact = 0.1;        // mmol/L drop per hour of basal

    double currentTime = 0;
    double glucose = currentGlucose;

    // Add initial glucose level
    predictions.append(qMakePair(currentTime, glucose));

    // Predict for every 5 minutes
    for (int i = 5; i <= timeSpanMinutes; i += 5) {
        // IOB decreases over time
        double iobDecay = 0.05; // 5% decay per 5 minutes
        insulinOnBoard *= (1.0 - iobDecay);

        // COB decreases over time
        double cobDecay = 0.1; // 10% decay per 5 minutes
        double carbsDigested = carbsOnBoard * cobDecay;
        carbsOnBoard -= carbsDigested;

        // Calculate glucose change
        double insulinEffect = (insulinOnBoard * iobDecay) * insulinSensitivity;
        double carbEffect = carbsDigested * carbSensitivity;
        double basalEffect = (basalRate / 12) * basalImpact; // 5 min = 1/12 of an hour

        // Update glucose level
        glucose += carbEffect - insulinEffect - basalEffect;

        // Add prediction point
        predictions.append(qMakePair(static_cast<double>(i), glucose));
    }

    return predictions;
}

// Sets the thresholds for low and high glucose alerts
void CGMManager::setAlerts(double lowGlucoseThreshold, double highGlucoseThreshold) {
    m_lowGlucoseThreshold = lowGlucoseThreshold;
    m_highGlucoseThreshold = highGlucoseThreshold;
}

// Checks whether the current glucose value triggers a low or high alert
bool CGMManager::checkAlerts(double currentGlucose) {
    bool isAlert = false;

    if (currentGlucose <= m_lowGlucoseThreshold) {
        qDebug() << "LOW GLUCOSE ALERT: " << currentGlucose << " mmol/L";
        isAlert = true;
    } else if (currentGlucose >= m_highGlucoseThreshold) {
        qDebug() << "HIGH GLUCOSE ALERT: " << currentGlucose << " mmol/L";
        isAlert = true;
    }

    return isAlert;
}

// Returns the most recent glucose reading, or a default if no readings exist
CGMManager::GlucoseReading CGMManager::getLatestReading() const {
    if (!m_readings.isEmpty()) {
        return m_readings.last();
    } else {
        // Return default reading if history is empty
        GlucoseReading empty;
        empty.timestamp = QDateTime::currentDateTime();
        empty.value = 0.0;
        empty.isAlarm = false;
        return empty;
    }
}

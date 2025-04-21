#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include "user.h"
#include <QtCharts/QValueAxis>
#include "BolusManager.h"
#include <QDebug>
#include <QVBoxLayout>
#include "CGMManager.h"
#include <QTimer>
#include <QRandomGenerator>
#include "SafetyController.h"
#include <QMessageBox>

QT_CHARTS_USE_NAMESPACE

namespace Ui {
class MainWindow;
}

// Main application window managing UI and simulation logic
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Handlers for bolus confirmation and delivery
    void on_bolusConfirmButton_clicked();
    void on_bolusCancelButton_clicked();
    void on_pushButton_ConfirmRequest_clicked();
    void on_pushButton_FinalDeliver_clicked();

private slots:
    // Core insulin calculation
    void calculateBolus();

    // Toggle extended bolus option
    void on_checkBox_Extended_toggled(bool checked);

    // Manual bolus calculation and confirmation
    void on_pushButton_Calculate_clicked();
    void on_pushButton_ConfirmBolus_clicked();
    void on_pushButton_ConfirmYes_clicked();

    // Profile management actions
    void on_pushButton_logInAccount_clicked();
    void on_pushButton_createAccount_clicked();
    void on_pushButton_updateAccount_clicked();
    void on_pushButton_deleteAccount_clicked();
    void populateCalculationFromProfile(Profile* profile);

    // Navigation and UI interactions
    void on_pushButton_ViewUnits_clicked();
    void on_pushButton_FinalDeliver_3_clicked();
    void on_pushButton_calcNext_clicked();
    void on_pushButton_confBack_clicked();
    void on_pushButton_confNext_clicked();
    void on_pushButton_bolusInitBack_clicked();

    // Handlers for extended bolus spinboxes
    void on_ExtendedNowChanged(int value);
    void on_ExtendedLaterChanged(int value);

    void on_pushButton_extendedConf_clicked();
    void on_pushButton_CancelConfirm_2_clicked();
    void on_pushButton_summaryConfirm_clicked_clicked();

    // CGM interaction
    void on_pushButton_StartCGM_2_clicked();
    void on_pushButton_calcBack_2_clicked();
    void on_pushButton_BackToConfirm_4_clicked();
    bool canAutoCorrect();

    // Stop ongoing insulin delivery
    void on_pushButton_StopDelivery_clicked();
    void on_pushButton_calcBack_clicked();

private:
    Ui::MainWindow *ui;

    // User-specific insulin settings
    double insulinToCarbRatio;
    double correctionFactor;
    double targetBG;
    double insulinOnBoard;

    BolusManager bolusManager;       // Bolus calculation/delivery logic
    User user;                       // Current user account
    CGMManager *cgmManager;         // Continuous Glucose Monitor logic

    // Glucose chart components
    QChart *glucoseChart = nullptr;
    QLineSeries *glucoseSeries = nullptr;
    QLineSeries *predictionSeries = nullptr;
    QValueAxis *glucoseAxisX = nullptr;
    QValueAxis *glucoseAxisY = nullptr;
    QChartView *chartView = nullptr;

    // Chart and CGM data management
    void setupGlucoseChart();
    void updateGlucoseChart(double time, double glucoseLevel);
    void updateCGMDisplay();
    void handleCGMReading(double glucoseLevel);
    void startCGMSimulation();
    void updatePredictions(double currentTime, double currentGlucose);
    int cgmSimElapsed = 0;     // How many seconds have passed
    int cgmSimDuration = 0;    // Total seconds to simulate
    int simulatedMinutesElapsed = 0;
    QTime simulatedStartTime;
    int correctionUnits;
    QDateTime lastAutoCorrectionTime;
    int timeElapsed = 5; // Simulated minutes (starts at 5)
    int insulinLevel;
    bool lowInsulinWarned;

    QTimer *cgmSimulationTimer;     // Timer to simulate CGM readings

    // Safety and warning mechanisms
    SafetyController *controller;
    QTimer *insulinDrainTimer;
    bool hasShownLowInsulinWarning = false;
    bool cancelExtendedBolus = false;
};

#endif // MAINWINDOW_H

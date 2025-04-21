#include "mainwindow.h"
#include "ui_mainwindow.h"

QT_CHARTS_USE_NAMESPACE

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      controller(new SafetyController(this))
{
    ui->setupUi(this);

    // UI visibility setup
    ui->correctionPrompt->setVisible(false);
    ui->groupBox_DeliverBolus->setVisible(false);
    ui->groupBox_ExtendedOptions->setVisible(false);

    // Connect bolus and extended spinbox logic
    connect(ui->pushButton_FinalDeliver, &QPushButton::clicked, this, &MainWindow::on_pushButton_FinalDeliver_clicked);
    connect(ui->spinBox_ExtendedNow, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_ExtendedNowChanged);
    connect(ui->checkBox_Extended, &QCheckBox::toggled, this, &MainWindow::on_checkBox_Extended_toggled);
    connect(ui->spinBox_ExtendedLater, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_ExtendedLaterChanged);

    ui->spinBox_ExtendedNow->setRange(0, 100);
    ui->spinBox_ExtendedLater->setRange(0, 100);

    // CGM setup
    cgmManager = new CGMManager(&bolusManager);
    cgmSimulationTimer = new QTimer(this);
    connect(cgmSimulationTimer, &QTimer::timeout, this, &MainWindow::updateCGMDisplay);

    connect(ui->pushButton_StartCGM, &QPushButton::clicked, this, &MainWindow::startCGMSimulation);
    connect(ui->pushButton_StopCGM, &QPushButton::clicked, this, [this]() {
        cgmSimulationTimer->stop();
        ui->label_CGMStatus->setText("CGM Monitoring: Stopped");
    });

    setupGlucoseChart(); // Initialize chart on startup

    // Battery progress bar setup
    ui->batteryProgressBar->setValue(100);
    ui->batteryProgressBar->setStyleSheet("QProgressBar::chunk { background-color: green; }");

    // Battery level updates and visual cues
    connect(controller, &SafetyController::batteryLevelUpdated, this, [=](int level) {
        ui->batteryProgressBar->setValue(level);
        if (level <= 20)
            ui->batteryProgressBar->setStyleSheet("QProgressBar::chunk { background-color: red; }");
        else if (level <= 50)
            ui->batteryProgressBar->setStyleSheet("QProgressBar::chunk { background-color: orange; }");
        else
            ui->batteryProgressBar->setStyleSheet("QProgressBar::chunk { background-color: green; }");
    });

    connect(controller, &SafetyController::triggerBatteryAlert, this, [=]() {
        QMessageBox::warning(this, "Low Battery", "LOW BATTERY: Please recharge the pump.");
    });

    connect(ui->chargeButton, &QPushButton::clicked, this, [=]() {
        controller->rechargeBattery();
    });

    connect(controller, &SafetyController::batteryDepleted, this, [=]() {
        QMessageBox::critical(this, "Shut down", "Device shutting down – NO BATTERY");
        QApplication::quit();
    });

    // Insulin progress bar updates
    connect(controller, &SafetyController::insulinLevelUpdated, this, [=](int level) {
        qDebug() << "[UI] Updating insulinProgressBar:" << level << "units";
        ui->insulinProgressBar->setValue(level);
        if (level <= 50)
            ui->insulinProgressBar->setStyleSheet("QProgressBar::chunk { background-color: red; }");
        else if (level <= 100)
            ui->insulinProgressBar->setStyleSheet("QProgressBar::chunk { background-color: orange; }");
        else
            ui->insulinProgressBar->setStyleSheet("QProgressBar::chunk { background-color: green; }");
    });

    ui->insulinProgressBar->setRange(0, 200);
    ui->insulinProgressBar->setValue(100);


    connect(controller, &SafetyController::triggerLowInsulinAlert, this, [=]() {
        QMessageBox::critical(this, "Low Insulin", "LOW INSULIN: Please refill insulin.");
    });

    // Cancel ongoing bolus delivery
    connect(ui->pushButton_StopDelivery, &QPushButton::clicked, this, [=]() {
        cancelExtendedBolus = true;
        QMessageBox::information(this, "Delivery Stopped", "Extended bolus delivery has been canceled.");
    });

    double newIOB = qMax(0.0, ui->doubleSpinBox_IOB->value() - 0.1); // Decrease IOB every 5 min
    ui->doubleSpinBox_IOB->setValue(newIOB);

    // Refill insulin bar
    connect(ui->pushButton_RefillInsulin, &QPushButton::clicked, this, [=]() {
        controller->refillInsulin();
    });

}

MainWindow::~MainWindow()
{
    delete ui;
}

// Handles bolus confirmation from user
void MainWindow::on_bolusConfirmButton_clicked() {
    double carbs = ui->doubleSpinBox_Carbs->value();
    double bg = ui->doubleSpinBox_BG->value();
    double icr = ui->doubleSpinBox_ICR->value();
    double cf = ui->doubleSpinBox_CF->value();
    double targetBG = ui->doubleSpinBox_TargetBG->value();
    double iob = ui->doubleSpinBox_IOB->value();

    auto result = bolusManager.calculateBolus(carbs, bg, icr, cf, targetBG, iob);
    QString deliveryLog = bolusManager.deliverBolus(result, true);

    double updatedIOB = bolusManager.updateIOB(iob, result.finalBolus);
    ui->doubleSpinBox_IOB->setValue(updatedIOB);
    calculateBolus();  // Update UI labels
}

// Cancels current bolus
void MainWindow::on_bolusCancelButton_clicked() {
    QString log = bolusManager.cancelBolus();
}

// Shows/hides extended bolus options based on checkbox
void MainWindow::on_checkBox_Extended_toggled(bool checked) {
    ui->groupBox_ExtendedOptions->setVisible(checked);

    if (checked) {
        BolusResult result = bolusManager.getLastResult();
        double foodBolus = result.carbBolus;

        int nowPercent = ui->spinBox_Immediate->value();
        int laterPercent = 100 - nowPercent;

        ui->spinBox_ExtendedNow->blockSignals(true);
        ui->spinBox_ExtendedLater->blockSignals(true);

        ui->spinBox_ExtendedNow->setValue(nowPercent);
        ui->spinBox_ExtendedLater->setValue(laterPercent);
        ui->doubleSpinBox_ExtendedDuration->setValue(ui->doubleSpinBox_Hours->value());

        ui->spinBox_ExtendedNow->blockSignals(false);
        ui->spinBox_ExtendedLater->blockSignals(false);

        ui->label_ExtendedHeader->setText(QString("Delivering %1 u").arg(foodBolus, 0, 'f', 2));
    }
}

// Syncs spinboxes to total 100%
void MainWindow::on_ExtendedNowChanged(int value) {
    ui->spinBox_ExtendedLater->blockSignals(true);
    ui->spinBox_ExtendedLater->setValue(100 - value);
    ui->spinBox_ExtendedLater->blockSignals(false);
}

void MainWindow::on_ExtendedLaterChanged(int value) {
    ui->spinBox_ExtendedNow->blockSignals(true);
    ui->spinBox_ExtendedNow->setValue(100 - value);
    ui->spinBox_ExtendedNow->blockSignals(false);
}

// Calculates and updates bolus UI values
void MainWindow::calculateBolus() {
    double carbs = ui->doubleSpinBox_Carbs->value();
    double bg = ui->doubleSpinBox_BG->value();
    double iob = ui->doubleSpinBox_IOB->value();
    double icr = ui->doubleSpinBox_ICR->value();
    double cf = ui->doubleSpinBox_CF->value();
    double target = ui->doubleSpinBox_TargetBG->value();
    double fraction = ui->spinBox_Immediate->value() / 100.0;
    int hours = ui->doubleSpinBox_Hours->value();

    bool includeCorrection = (bg > target && ui->checkBox_IncludeCorrection->isChecked());
    double correctionBolus = includeCorrection ? (bg - target) / cf : 0.0;

    double carbBolus = carbs / icr;
    double totalBolus = carbBolus + correctionBolus;
    double finalBolus = totalBolus - iob;
    if (finalBolus < 0) finalBolus = 0;

    double immediate = finalBolus * fraction;
    double extended = finalBolus - immediate;
    double rate = (hours > 0) ? extended / hours : 0;

    ui->spinBox_ImmediateDose->setValue(immediate);
    ui->spinBox_ExtendedRate->setValue(rate);
    ui->spinBox_TotalBolus->setValue(finalBolus);

    ui->correctionPrompt->setVisible(bg > target);
}

void MainWindow::on_pushButton_ConfirmRequest_clicked() {
    // Gather user-entered values
    double carbs = ui->doubleSpinBox_Carbs->value();
    double bg = ui->doubleSpinBox_BG->value();
    double totalBolus = ui->spinBox_TotalBolus->value();

    // Update labels on the confirmation screen
    ui->label_ConfirmCarbs->setText(QString("%1 g").arg(carbs));
    ui->label_ConfirmBG->setText(QString("%1 mmol/L").arg(bg, 0, 'f', 1));
    ui->label_ConfirmUnits->setText(QString("%1 u").arg(totalBolus, 0, 'f', 2));

    // Show delivery panel and proceed
    ui->groupBox_DeliverBolus->setVisible(true);
    on_pushButton_ConfirmYes_clicked(); // Proceed with bolus breakdown display
}

void MainWindow::on_pushButton_Calculate_clicked() {
    // Recalculate bolus based on input values
    calculateBolus();
}

void MainWindow::on_pushButton_ConfirmBolus_clicked() {
    // Ensure bolus is up to date
    calculateBolus();

    // Reset extended bolus options
    ui->groupBox_ExtendedOptions->setVisible(false);
    ui->checkBox_Extended->setChecked(false);

    // Switch to confirmation view
    ui->stackedWidget->setCurrentWidget(ui->confirmationPage);

    // Set confirmation labels
    double carbs = ui->doubleSpinBox_Carbs->value();
    double bg = ui->doubleSpinBox_BG->value();
    double totalBolus = ui->spinBox_TotalBolus->value();
    ui->label_ConfirmCarbs->setText(QString("Carbs: %1 g").arg(carbs));
    ui->label_ConfirmBG->setText(QString("BG: %1 mmol/L").arg(bg, 0, 'f', 1));
    ui->label_ConfirmUnits->setText(QString("Units to Deliver: %1 u").arg(totalBolus, 0, 'f', 2));

    // Hide delivery box until explicitly confirmed
    ui->groupBox_DeliverBolus->setVisible(false);
}

void MainWindow::on_pushButton_ConfirmYes_clicked() {
    // Recalculate bolus using current settings
    double carbs = ui->doubleSpinBox_Carbs->value();
    double bg = ui->doubleSpinBox_BG->value();
    double icr = ui->doubleSpinBox_ICR->value();
    double cf = ui->doubleSpinBox_CF->value();
    double targetBG = ui->doubleSpinBox_TargetBG->value();
    double iob = ui->doubleSpinBox_IOB->value();
    double frac = ui->spinBox_Immediate->value() / 100.0;
    int hours = ui->doubleSpinBox_Hours->value();

    bolusManager.calculateBolus(carbs, bg, icr, cf, targetBG, iob, frac, hours);

    // Show bolus delivery box with detailed breakdown
    ui->groupBox_DeliverBolus->setVisible(true);
    BolusResult result = bolusManager.getLastResult();
    ui->label_DeliverAmountTitle->setText(QString("Deliver %1 u Bolus?").arg(result.finalBolus, 0, 'f', 2));
    ui->label_CorrectionAmount->setText(QString("Correction: %1 u").arg(result.correctionBolus, 0, 'f', 2));
    ui->label_FoodAmount->setText(QString("Food: %1 u").arg(result.carbBolus, 0, 'f', 2));
}

void MainWindow::on_pushButton_FinalDeliver_clicked() {
    BolusResult result = bolusManager.getLastResult();
    double finalBolus = result.finalBolus;
    double foodBolus = result.carbBolus;
    double fraction = ui->spinBox_Immediate->value() / 100.0;
    bool extended = ui->checkBox_Extended->isChecked();

    double immediate = finalBolus * fraction;

    if (extended) {
        // Set extended values for breakdown and duration
        ui->label_ExtendedHeader->setText(QString("Delivering %1 u").arg(foodBolus, 0, 'f', 2));
        ui->groupBox_ExtendedOptions->setVisible(true);

        int nowPercent = ui->spinBox_Immediate->value();
        int laterPercent = 100 - nowPercent;

        ui->spinBox_ExtendedNow->setValue(nowPercent);
        ui->spinBox_ExtendedLater->setValue(laterPercent);
        ui->doubleSpinBox_ExtendedDuration->setValue(ui->doubleSpinBox_Hours->value());

        return; // Wait for extended confirmation
    }

    // Immediate-only bolus delivery
    double updatedIOB = bolusManager.updateIOB(ui->doubleSpinBox_IOB->value(), finalBolus);
    ui->doubleSpinBox_IOB->setValue(updatedIOB);

    QString bolusText = QString("%1 u Now + %2 u Later")
                            .arg(immediate, 0, 'f', 3)
                            .arg(0.0, 0, 'f', 3);

    ui->label_BolusAmounts->setText(bolusText);
    ui->stackedWidget->setCurrentWidget(ui->bolusInitiatedPage);
}

void MainWindow::on_pushButton_ViewUnits_clicked() {
    // Navigate back to main bolus calculator
    ui->stackedWidget->setCurrentWidget(ui->calculationpage);
}

void MainWindow::on_pushButton_calcNext_clicked() {
    // Move to confirmation screen
    ui->stackedWidget->setCurrentWidget(ui->confirmationPage);
}

void MainWindow::on_pushButton_confBack_clicked() {
    // Return to calculator screen
    ui->stackedWidget->setCurrentWidget(ui->calculationpage);
}

void MainWindow::on_pushButton_confNext_clicked() {
    // Proceed to delivery initiated page
    ui->stackedWidget->setCurrentWidget(ui->bolusInitiatedPage);
}

void MainWindow::on_pushButton_bolusInitBack_clicked() {
    // Navigate back to confirmation
    ui->stackedWidget->setCurrentWidget(ui->confirmationPage);
}

void MainWindow::on_pushButton_extendedConf_clicked() {
    // Collect and compute extended bolus breakdown
    BolusResult result = bolusManager.getLastResult();
    double foodBolus = result.carbBolus;
    double correction = result.correctionBolus;
    double totalBolus = result.finalBolus;

    double percentNow = ui->spinBox_ExtendedNow->value();
    double percentLater = ui->spinBox_ExtendedLater->value();
    double hours = ui->doubleSpinBox_ExtendedDuration->value();

    double nowWithCorrection = (foodBolus * (percentNow / 100.0)) + correction;
    double later = foodBolus * (percentLater / 100.0);

    // Format duration as hh:mm
    int durationMinutes = static_cast<int>(hours * 60);
    int hrs = durationMinutes / 60;
    int mins = durationMinutes % 60;
    QString durationString = QString("%1:%2 hrs").arg(hrs).arg(mins, 2, 10, QChar('0'));

    // Populate summary labels
    ui->label_SummaryTotalBolus->setText(QString("Deliver %1 u Bolus?").arg(totalBolus, 0, 'f', 2));
    ui->label_SummaryNowCorrection->setText(QString("Now + Correction: %1 u").arg(nowWithCorrection, 0, 'f', 3));
    ui->label_SummaryLater->setText(QString("Later: %1 u").arg(later, 0, 'f', 3));
    ui->label_SummaryDuration->setText("Duration: " + durationString);

    ui->label_PercentNow->setText(QString("Deliver Now %1%").arg(percentNow, 0, 'f', 0));
    ui->label_PercentLater->setText(QString("Deliver Later %1%").arg(percentLater, 0, 'f', 0));
    ui->label_PercentDuration->setText("Duration: " + durationString);

    // Navigate to summary page
    ui->stackedWidget->setCurrentWidget(ui->extendedSummaryPage);
}

void MainWindow::on_pushButton_CancelConfirm_2_clicked() {
    // Return from summary to confirmation
    ui->stackedWidget->setCurrentWidget(ui->confirmationPage);
}

void MainWindow::on_pushButton_summaryConfirm_clicked_clicked() {
    // Finalize extended delivery and navigate to delivery page
    BolusResult result = bolusManager.getLastResult();

    double foodBolus = result.carbBolus;
    double correctionBolus = result.correctionBolus;

    double percentNow = ui->spinBox_ExtendedNow->value();
    double percentLater = ui->spinBox_ExtendedLater->value();

    double nowWithCorrection = (foodBolus * (percentNow / 100.0)) + correctionBolus;
    double later = foodBolus * (percentLater / 100.0);

    QString bolusText = QString("%1 u Now + %2 u Later")
                            .arg(nowWithCorrection, 0, 'f', 3)
                            .arg(later, 0, 'f', 3);

    ui->label_BolusAmounts->setText(bolusText);
    ui->stackedWidget->setCurrentWidget(ui->bolusInitiatedPage);
}

void MainWindow::on_pushButton_FinalDeliver_3_clicked() {
    cancelExtendedBolus = false;

    QString labelText = ui->label_BolusAmounts->text();

    // Split on "+"
    QStringList parts = labelText.split("+");
    double nowUnits = 0.0, laterUnits = 0.0;

    if (parts.size() == 2) {
        // Extract "X u Now", trim, split by space, get first token
        QString nowPart = parts[0].trimmed();
        QString laterPart = parts[1].trimmed();

        nowUnits = nowPart.split(" ").first().toDouble();
        laterUnits = laterPart.split(" ").first().toDouble();
    }

    double totalUnits = nowUnits + laterUnits;

    // Simulate delivery after 3 sec
    QTimer::singleShot(3000, this, [=]() {
        if (!cancelExtendedBolus) {
            controller->registerInsulinDelivery(-totalUnits);

            QMessageBox *msg = new QMessageBox(this);
            msg->setIcon(QMessageBox::Information);
            msg->setWindowTitle("Bolus Delivery");
            msg->setText(QString("Delivered %1 u insulin.").arg(totalUnits, 0, 'f', 2));
            msg->setStandardButtons(QMessageBox::Ok);

            connect(msg, &QMessageBox::accepted, this, [=]() {
                ui->stackedWidget->setCurrentWidget(ui->calculationpage);
            });

            msg->exec();
        }
    });

    double totalBolus = ui->spinBox_TotalBolus->value();
    ui->label_ManualRequest->setText(QString("Requesting %1 u Bolus").arg(totalBolus, 0, 'f', 2));
    ui->stackedWidget->setCurrentWidget(ui->manualBolusPage);
}



// Glucose Chart Setup
void MainWindow::setupGlucoseChart() {
    // Clean up existing chart (if reinitializing)
    if (glucoseChart) {
        delete glucoseChart;
        delete chartView;
    }

    // Series for current glucose values (blue solid line)
    glucoseSeries = new QLineSeries();
    glucoseSeries->setName("Current Glucose");
    glucoseSeries->setColor(QColor(0, 0, 255));
    QPen glucosePen(QColor(0, 0, 255));
    glucosePen.setWidth(2);
    glucoseSeries->setPen(glucosePen);

    // Series for predicted glucose values (light blue dashed line)
    predictionSeries = new QLineSeries();
    predictionSeries->setName("Predicted Glucose");
    predictionSeries->setColor(QColor(100, 100, 255, 150));
    QPen predictionPen(QColor(100, 100, 255, 150));
    predictionPen.setWidth(2);
    predictionPen.setStyle(Qt::DashLine);
    predictionSeries->setPen(predictionPen);

    // Create chart and add series
    glucoseChart = new QChart();
    glucoseChart->setTitle("Glucose Levels Over Time");
    glucoseChart->addSeries(glucoseSeries);
    glucoseChart->addSeries(predictionSeries);

    // X-axis: Time (minutes)
    glucoseAxisX = new QValueAxis();
    glucoseAxisX->setTitleText("Time (minutes)");
    glucoseAxisX->setLabelFormat("%d");
    glucoseAxisX->setTickCount(7);
    glucoseAxisX->setRange(0, 60);
    glucoseChart->addAxis(glucoseAxisX, Qt::AlignBottom);
    glucoseSeries->attachAxis(glucoseAxisX);
    predictionSeries->attachAxis(glucoseAxisX);

    // Y-axis: Glucose level (mmol/L)
    glucoseAxisY = new QValueAxis();
    glucoseAxisY->setTitleText("Glucose Level (mmol/L)");
    glucoseAxisY->setLabelFormat("%.1f");
    glucoseAxisY->setRange(3.0, 12.0); // Normal range
    glucoseChart->addAxis(glucoseAxisY, Qt::AlignLeft);
    glucoseSeries->attachAxis(glucoseAxisY);
    predictionSeries->attachAxis(glucoseAxisY);

    // Add green dotted target low line at 4.0 mmol/L
    double targetLow = 4.0;
    QLineSeries *targetLowLine = new QLineSeries();
    targetLowLine->append(0, targetLow);
    targetLowLine->append(360, targetLow);
    targetLowLine->setName("Target Low");
    QPen lowPen(QColor(0, 180, 0));
    lowPen.setWidth(1);
    lowPen.setStyle(Qt::DotLine);
    targetLowLine->setPen(lowPen);
    glucoseChart->addSeries(targetLowLine);
    targetLowLine->attachAxis(glucoseAxisX);
    targetLowLine->attachAxis(glucoseAxisY);

    // Add red dotted target high line at 7.0 mmol/L
    double targetHigh = 7.0;
    QLineSeries *targetHighLine = new QLineSeries();
    targetHighLine->append(0, targetHigh);
    targetHighLine->append(360, targetHigh);
    targetHighLine->setName("Target High");
    QPen highPen(QColor(180, 0, 0));
    highPen.setWidth(1);
    highPen.setStyle(Qt::DotLine);
    targetHighLine->setPen(highPen);
    glucoseChart->addSeries(targetHighLine);
    targetHighLine->attachAxis(glucoseAxisX);
    targetHighLine->attachAxis(glucoseAxisY);

    // Create chart view with antialiasing
    chartView = new QChartView(glucoseChart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Show legend at the bottom
    glucoseChart->legend()->setVisible(true);
    glucoseChart->legend()->setAlignment(Qt::AlignBottom);

    // Replace previous content in chart container
    QVBoxLayout *layout = new QVBoxLayout(ui->chartContainer);
    if (layout->count() > 0) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
    }

    layout->addWidget(chartView);
    ui->chartContainer->setLayout(layout);
}

// CGM Monitoring
void MainWindow::startCGMSimulation() {
    timeElapsed = 0;

    simulatedMinutesElapsed = 0;
    simulatedStartTime = QTime(0, 0); // Start at 00:00

    // Initialize chart if needed
    if (!glucoseChart) {
        setupGlucoseChart();
    }

    // Clear chart data
    glucoseSeries->clear();
    predictionSeries->clear();

    // Use BG input or generate random initial value
    double initialGlucose = ui->doubleSpinBox_BG->value();
    if (initialGlucose <= 0.0) {
        initialGlucose = 4.0 + (QRandomGenerator::global()->generateDouble() * 6.0); // 4–10 mmol/L
        initialGlucose = qRound(initialGlucose * 10) / 10.0;
        qDebug() << "Generated initial glucose:" << initialGlucose;
    }

    // Start at time = 0 minutes
    double currentTime = 0;
    updateGlucoseChart(currentTime, initialGlucose);
    handleCGMReading(initialGlucose); // Process reading (alerts, logging, etc.)

    // Simulate CGM: 1 update per 5 seconds = 5 minutes real-time
    //cgmSimulationTimer->start(5000);

    // Determine user-selected duration
    QString selected = ui->comboBox_CGM_Duration->currentText();
    if (selected == "1 hour") cgmSimDuration = 12;
    else if (selected == "3 hours") cgmSimDuration = 36;
    else if (selected == "6 hours") cgmSimDuration = 72;
    else cgmSimDuration = 12; // fallback default

    cgmSimElapsed = 0;
    cgmSimulationTimer->start(1000); // 1 real second = 5 min simulated

    // UI updates
    ui->label_CGMStatus->setText("CGM Monitoring: Active");
    ui->label_CGMStatus->setStyleSheet("");
    ui->stackedWidget->setCurrentWidget(ui->cgmPage);

    // Log simulation start
    ui->plainTextEdit_CGMLogs->appendPlainText(
        QString("[%1] CGM Monitoring Started - Initial BG: %2 mmol/L")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(initialGlucose, 0, 'f', 1)
    );

    QTimer *insulinUseTimer = new QTimer(this);
    connect(insulinUseTimer, &QTimer::timeout, this, [=]() {
        controller->registerInsulinDelivery(3);  // arbitrary insulin drop per interval
    });
    insulinUseTimer->start(1000); // 1 sec = 5 min sim

}

void MainWindow::updateCGMDisplay() {
    // Stops display after duration has fully passed
    cgmSimElapsed++;
    if (cgmSimElapsed > cgmSimDuration) {
        cgmSimulationTimer->stop();
        ui->label_CGMStatus->setText("CGM Simulation Complete");
        return;
    }

    double currentBG = ui->doubleSpinBox_BG->value();

    double insulinEffect = ui->doubleSpinBox_IOB->value() * 0.08;
    double carbEffect = ui->doubleSpinBox_Carbs->value() * 0.008;

    // Decay logic
    double newIOB = qMax(0.0, ui->doubleSpinBox_IOB->value() - 0.1);
    ui->doubleSpinBox_IOB->setValue(newIOB);

    double newCarbs = qMax(0.0, ui->doubleSpinBox_Carbs->value() - 0.2);
    ui->doubleSpinBox_Carbs->setValue(newCarbs);

    double randomVariation = ((QRandomGenerator::global()->bounded(60) - 30) * 0.01);

    // Compute new BG value
    double newBG = currentBG + carbEffect - insulinEffect + randomVariation;

    // Clamp to safe physiological bounds
    if (newBG < 2.5) newBG = 2.5;
    if (newBG > 20.0) newBG = 20.0;

    // Advance time
    timeElapsed += 5;
    simulatedMinutesElapsed += 5; // Each update = 5 minutes of simulated time

    // Extend chart if needed
    if (timeElapsed >= glucoseAxisX->max()) {
        glucoseAxisX->setRange(0, glucoseAxisX->max() + 30);
    }

    // Simulate basal insulin use
    int drop = (timeElapsed / 5) % 12; // simulate 12 values
    int simDrop = (drop == 4 || drop == 8 || drop == 12) ? 3 : 3;
    controller->registerInsulinDelivery(-simDrop);

    // Update displayed BG and chart
    ui->doubleSpinBox_BG->setValue(newBG);
    updateGlucoseChart(timeElapsed, newBG);
    handleCGMReading(newBG); // Check for alerts
}

bool MainWindow::canAutoCorrect() {
    QDateTime now = QDateTime::currentDateTime();
    static QDateTime lastCorrectionTime = now.addSecs(-1800); // Initialize in past

    int secondsSinceLast = lastCorrectionTime.secsTo(now);
    if (secondsSinceLast < 1800) return false;

    lastCorrectionTime = now;
    return true;
}

void MainWindow::handleCGMReading(double glucoseLevel) {

    double newBG = glucoseLevel;
    double IOB = ui->doubleSpinBox_IOB->value();
    double carbs = ui->doubleSpinBox_Carbs->value();
    double basalRate = ui->spinBox_Basal->value();
    double CF = ui->doubleSpinBox_CF->value(); // correction factor
    QDateTime now = QDateTime::currentDateTime();

    ui->label_CurrentBG->setText(QString("%1 mmol/L").arg(glucoseLevel, 0, 'f', 1));

    // Log raw values used for decision making
    double targetBG = ui->doubleSpinBox_TargetBG->value();
    double cf = ui->doubleSpinBox_CF->value();
    QString simTime = QString("%1:%2")
        .arg(timeElapsed / 60, 2, 10, QChar('0'))  // hours
        .arg(timeElapsed % 60, 2, 10, QChar('0')); // minutes


    qDebug() << "[DEBUG] BG =" << glucoseLevel
             << "| Target BG =" << targetBG
             << "| CF =" << cf;

    // Alert Label Logic
    if (glucoseLevel <= 3.9) {
        ui->label_CGMStatus->setText("ALERT: Low Glucose");
        ui->label_CGMStatus->setStyleSheet("color: red; font-weight: bold;");
    } else if (glucoseLevel >= 10.0) {
        ui->label_CGMStatus->setText("ALERT: High Glucose");
        ui->label_CGMStatus->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->label_CGMStatus->setText("CGM Monitoring: Active");
        ui->label_CGMStatus->setStyleSheet("");
    }

    QString logEntry = QString("[%1] BG: %2 mmol/L").arg(simTime).arg(glucoseLevel, 0, 'f', 1);

    // Recommend Correction
    if (cf > 0.0 && glucoseLevel > targetBG + 2.0) {
        double correction = (glucoseLevel - targetBG) / cf;
        correction = std::round(correction * 100.0) / 100.0;

        qDebug() << "[DEBUG] Triggering Correction | Correction Dose:" << correction;

        logEntry += QString(" - Recommend correction: %1 u").arg(correction, 0, 'f', 2);

        int correctionUnits = static_cast<int>(correction);
        controller->registerInsulinDelivery(correctionUnits);

        ui->plainTextEdit_CGMLogs->appendPlainText(
            QString("[%1] Insulin Delivered (Auto): %2 u").arg(simTime).arg(correctionUnits)
        );
        ui->plainTextEdit_CGMLogs->appendPlainText("Recommended Correction Dose Administered");
    }
    // Recommend Carbs
    else if (glucoseLevel < targetBG - 1.0) {
        qDebug() << "[DEBUG] BG below target - recommending carbs.";
        logEntry += " - Consider carb intake";
    }
    // No action
    else {
        qDebug() << "[DEBUG] BG within acceptable range - no action taken.";
    }

    // CRITICAL warning logs
    if (glucoseLevel <= 2.8)
        ui->plainTextEdit_CGMLogs->appendPlainText(QString("[%1] CRITICAL: BG dangerously low!").arg(simTime));
    if (glucoseLevel >= 15.0)
        ui->plainTextEdit_CGMLogs->appendPlainText(QString("[%1] CRITICAL: BG dangerously high!").arg(simTime));

    // Final event log
    qDebug() << "[CGM] LogEntry Created:" << logEntry;
    ui->plainTextEdit_CGMLogs->appendPlainText(logEntry);

    double predictedBG30min = cgmManager->predictGlucoseLevels(newBG, IOB, carbs, basalRate, 30).last().second;

    if (predictedBG30min <= 3.9) {
        controller->setBasalRate(0.0);
        ui->plainTextEdit_CGMLogs->appendPlainText(QString("[%1] Suspended due to predicted low BG").arg(simTime));
    }
    else if (predictedBG30min <= 5.0) {
        controller->adjustBasalRate(-0.3);
        ui->plainTextEdit_CGMLogs->appendPlainText(QString("[%1] Basal rate decreased").arg(simTime));
    }
    else if (predictedBG30min >= 8.9) {
        controller->adjustBasalRate(+0.3);
        ui->plainTextEdit_CGMLogs->appendPlainText(QString("[%1] Basal rate increased").arg(simTime));
    }

    QDateTime timeNow = QDateTime::currentDateTime();
    int minsSinceLastAuto = lastAutoCorrectionTime.isValid() ? lastAutoCorrectionTime.secsTo(timeNow) / 60 : 999;

    if (predictedBG30min >= 10.0 && minsSinceLastAuto >= 60) {
        double correction = (predictedBG30min - targetBG) / CF;
        if (correction > 6.0) correction = 6.0;

        int correctionUnits = static_cast<int>(correction);
        controller->registerInsulinDelivery(correctionUnits);

        lastAutoCorrectionTime = timeNow;

        ui->plainTextEdit_CGMLogs->appendPlainText(
            QString("[%1] Auto correction bolus: %2 u").arg(simTime).arg(correctionUnits)
        );
    }


}


void MainWindow::updateGlucoseChart(double time, double glucoseLevel) {
    if (!glucoseSeries) return;

    glucoseSeries->append(time, glucoseLevel); // Plot new data point

    // Update axis limits based on current reading
    double maxY = qMax(glucoseAxisY->max(), glucoseLevel + 1.0);
    double minY = qMin(glucoseAxisY->min(), glucoseLevel - 1.0);

    // Clamp axis ranges to reasonable glucose values
    if (maxY < 12.0) maxY = 12.0;
    if (minY > 3.0) minY = 3.0;
    glucoseAxisY->setRange(qMax(2.0, minY - 0.5), qMin(20.0, maxY + 1.0));
    //glucoseAxisY->setRange(minY, maxY);

    updatePredictions(time, glucoseLevel); // Refresh prediction line
}

void MainWindow::updatePredictions(double currentTime, double currentGlucose) {
    predictionSeries->clear();
    predictionSeries->append(currentTime, currentGlucose);

    double insulinOnBoard = ui->doubleSpinBox_IOB->value();
    double carbsOnBoard = ui->doubleSpinBox_Carbs->value();

    double insulinDecayRate = 0.15;
    double carbDecayRate = 0.015;

    for (int i = 5; i <= 60; i += 5) {
        double timePoint = currentTime + i;

        // Linear decay of insulin & carbs
        double insulinEffect = insulinOnBoard * insulinDecayRate * (1.0 - (i / 60.0));
        double carbEffect = carbsOnBoard * carbDecayRate * (1.0 - (i / 60.0));

        if (insulinEffect < 0) insulinEffect = 0;
        if (carbEffect < 0) carbEffect = 0;

        double predictedGlucose = currentGlucose + carbEffect - insulinEffect;

        // Small smoothing oscillation
        double hoverOffset = 0.1 * std::sin(timePoint / 60.0 * 2 * M_PI);
        predictedGlucose += hoverOffset;

        // Clamp prediction
        if (predictedGlucose < 2.5)
            predictedGlucose = 2.5;
        else if (predictedGlucose > 15.0)
            predictedGlucose = 15.0;

        predictionSeries->append(timePoint, predictedGlucose);
    }
}


void MainWindow::on_pushButton_StopDelivery_clicked() {
    ui->plainTextEdit_CGMLogs->appendPlainText("Manual bolus delivery stopped by user.");
    ui->stackedWidget->setCurrentWidget(ui->calculationpage); // Return to calculator
}

void MainWindow::on_pushButton_StartCGM_2_clicked() {
    ui->stackedWidget->setCurrentWidget(ui->calculationpage); // Go back to calculation page
}

void MainWindow::on_pushButton_calcBack_2_clicked() {
    ui->stackedWidget->setCurrentWidget(ui->cgmPage); // Go back to CGM view
}

void MainWindow::on_pushButton_BackToConfirm_4_clicked() {
    ui->stackedWidget->setCurrentWidget(ui->bolusInitiatedPage); // Go back to bolus page
}

void MainWindow::on_pushButton_logInAccount_clicked() {
    QListWidgetItem *selectedItem = ui->listWidget_Accounts->currentItem();
    if (!selectedItem) {
        qDebug() << "[Login] No account selected!";
        return;
    }

    QString username = selectedItem->text();
    qDebug() << "[Login] Attempting to log in with username:" << username;

    // Read spinboxes from profile page (values manually set before login)
    double basalRate_input = ui->spinBox_Basal->value();
    double icr_input = ui->spinBox_ICR_2->value();
    double cf_input = ui->spinBox_CF_2->value();
    double target_input = ui->spinBox_TargetBG_2->value();

    qDebug() << "[Login] Spinbox values BEFORE login:";
    qDebug() << "  Basal Rate:" << basalRate_input;
    qDebug() << "  Carb Ratio (ICR):" << icr_input;
    qDebug() << "  Correction Factor (CF):" << cf_input;
    qDebug() << "  Target BG:" << target_input;

    // Save these values into the profile before setting active
    user.createOrUpdateProfile(username, basalRate_input, icr_input, cf_input, target_input);

    // Set active profile
    user.setActiveProfile(username);
    Profile* current = user.getActiveProfile();

    if (current) {
        qDebug() << "[Login] Retrieved profile from user object:";
        qDebug() << "  Basal Rate:" << current->basalRate;
        qDebug() << "  Carb Ratio (ICR):" << current->carbRatio;
        qDebug() << "  Correction Factor (CF):" << current->correctionFactor;
        qDebug() << "  Target BG:" << current->targetBG;

        // Update spinboxes to reflect the profile (should match what was just saved)
        ui->spinBox_Basal->setValue(current->basalRate);
        ui->spinBox_ICR_2->setValue(current->carbRatio);
        ui->spinBox_CF_2->setValue(current->correctionFactor);
        ui->spinBox_TargetBG_2->setValue(current->targetBG);

        // Update calculation page
        ui->doubleSpinBox_ICR->setValue(current->carbRatio);
        ui->doubleSpinBox_CF->setValue(current->correctionFactor);
        ui->doubleSpinBox_TargetBG->setValue(current->targetBG);

        qDebug() << "[Login] Spinboxes updated across both pages.";
    } else {
        qDebug() << "[Login] Profile not found!";
    }

    ui->stackedWidget->setCurrentWidget(ui->calculationpage);
}

void MainWindow::on_pushButton_createAccount_clicked() {
    QString username = ui->plainTextEdit->toPlainText();

    double basalRate = ui->spinBox_Basal->value();
    double carbRatio = ui->spinBox_ICR_2->value();
    double correctionFactor = ui->spinBox_CF_2->value();
    double targetBG = ui->spinBox_TargetBG_2->value();

    // Save profile under username
    bool created = user.editProfile(username, basalRate, carbRatio, correctionFactor, targetBG);
    if (created) {
        qDebug() << "Profile created for:" << username;
    }

    // Add to account list if not already there
    QList<QListWidgetItem*> items = ui->listWidget_Accounts->findItems(username, Qt::MatchExactly);
    if (items.isEmpty()) {
        ui->listWidget_Accounts->addItem(username);
    }
}

void MainWindow::on_pushButton_updateAccount_clicked()
{
    QString profileName = ui->comboBox_ProfileSelector->currentText();
    double basalRate = ui->spinBox_Basal->value();
    double carbRatio = ui->doubleSpinBox_ICR->value();
    double correction = ui->doubleSpinBox_CF->value();
    double target = ui->doubleSpinBox_TargetBG->value();

    bool updated = user.editProfile(profileName, basalRate, carbRatio, correction, target);

    if (updated) {
        qDebug() << "Profile updated:" << profileName;
    } else {
        qDebug() << "Profile update failed.";
    }
}


void MainWindow::on_pushButton_deleteAccount_clicked()
{
    QString username = ui->listWidget_Accounts->currentItem()->text();
    QString profileName = ui->comboBox_ProfileSelector->currentText();

    user.deleteProfile(profileName);

    // Remove from UI
    delete ui->listWidget_Accounts->currentItem();
    int index = ui->comboBox_ProfileSelector->findText(profileName);
    if (index != -1) ui->comboBox_ProfileSelector->removeItem(index);

    qDebug() << "Profile deleted:" << profileName;
}

void MainWindow::on_pushButton_calcBack_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->profilelogin); // Go back to profile page
}

void MainWindow::populateCalculationFromProfile(Profile* profile) {
    if (!profile) return;

    // Ensures spinbox values in calculationpage correspond to values from profile page
    ui->doubleSpinBox_ICR->setValue(profile->carbRatio);
    ui->doubleSpinBox_CF->setValue(profile->correctionFactor);
    ui->doubleSpinBox_TargetBG->setValue(profile->targetBG);
}

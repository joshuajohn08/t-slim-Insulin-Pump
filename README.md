# COMP-3004-FP

t:slim X2 Insulin Pump Simulator (Qt + C++)

Overview:
This project simulates the t:slim X2 insulin pump, providing a GUI-based experience that includes bolus calculation, profile management, CGM monitoring, insulin/battery levels, and safety alerts. It is built using Qt 5 and the Qt Charts module for visualization.

Required Dependencies:
Before building, ensure the Qt Charts module is installed using the following command:
sudo apt install libqt5charts5-dev

How to Build and Run in Qt Creator:
1. Open Qt Creator.
2. Select 'Open Project' and choose the .pro file or CMakeLists.txt if available.
3. Configure the project with your preferred kit
4. Click the 'Run' button or use Ctrl+R to compile and execute the application.

File Descriptions:

Headers:
BolusManager.h - Declares the BolusManager class responsible for calculating insulin doses based on user inputs such as carbs, BG, ICR, correction factor, and insulin on board.
CGMManager.h - Declares the CGMManager class which simulates CGM readings, applying random variations and trend predictions based on insulin and carb inputs.
mainwindow.h - Declares the MainWindow class which manages the GUI, page navigation, UI updates, and integrates all functional components.
SafetyController.h - Declares the SafetyController class which monitors and triggers alerts for battery and insulin levels.
user.h - Declares the User class and Profile struct for managing user-specific insulin settings such as carb ratio, correction factor, target BG, and basal rate.

Sources:
BolusManager.cpp - Implements insulin bolus calculation logic, including carb bolus, correction bolus, and IOB adjustment.
CGMManager.cpp - Implements simulated glucose readings and prediction logic for the CGM chart based on insulin/carbs over time.
main.cpp - Entry point of the application. Initializes and displays the main window.
mainwindow.cpp - Implements all GUI-related behavior including event handling for bolus delivery, profile management, CGM chart setup, and safety alerts.
SafetyController.cpp - Implements logic for battery drain, insulin level decay, and related UI alerts.
user.cpp - Implements profile creation, editing, deletion, and syncing between profile login and bolus calculation pages.

Forms:
mainwindow.ui - Contains the GUI layout for all stacked pages including the profile manager, bolus calculator, confirmation screen, and CGM monitoring page.

Members & Responsiblities:

Arhaan Wazid - Developer
Holly Phamisith - Developer + Diagrams
Nooreen Pasha - Diagrams
Shafia Ahmed - Developer + Diagrams
Joshua John - Developer + Diagrams

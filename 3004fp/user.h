#ifndef USER_H
#define USER_H

#include <QString>
#include <vector>
#include <map>

// Represents an insulin delivery profile
struct Profile {
    QString name;
    double basalRate;          // Basal insulin rate (units/hour)
    double carbRatio;          // Carbohydrate ratio (grams/unit)
    double correctionFactor;   // Correction factor (mg/dL per unit)
    double targetBG;           // Target blood glucose level
};

// Manages user profiles and active profile selection
class User {
public:
    User(); // Initializes the user with 3 default profiles

    // Edit an existing profile or update its values
    bool editProfile(const QString& profileName, double basalRate, double carbRatio, double correctionFactor, double targetBG);

    // Retrieve a pointer to a specific profile
    Profile* getProfile(const QString& profileName);

    // Get a list of all profile names
    std::vector<QString> getAllProfileNames() const;

    // Set the active profile for calculations/delivery
    bool setActiveProfile(const QString& profileName);

    // Get the currently active profile
    Profile* getActiveProfile() const;

    // Delete a user profile
    bool deleteProfile(const QString& profileName);

private:
    std::map<QString, Profile> profiles;  // Stores all user profiles
    QString activeProfile;                // Name of the active profile
};

#endif // USER_H

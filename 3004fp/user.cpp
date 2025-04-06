#include "user.h"

// Constructor initializes 3 default user profiles with predefined settings
User::User() {
    profiles["Morning Routine"] = {"Morning Routine", 1.2, 12.5, 50.0, 5.5};
    profiles["Exercise Mode"] = {"Exercise Mode", 0.8, 10.0, 45.0, 6.0};
    profiles["Night Routine"] = {"Night Routine", 1.0, 11.0, 48.0, 5.0};

    activeProfile = "Morning Routine"; // Set default active profile
}

// Edits an existing profile with new insulin parameters
bool User::editProfile(const QString& profileName, double basalRate, double carbRatio, double correctionFactor, double targetBG) {
    auto it = profiles.find(profileName);
    if (it != profiles.end()) {
        it->second.basalRate = basalRate;
        it->second.carbRatio = carbRatio;
        it->second.correctionFactor = correctionFactor;
        it->second.targetBG = targetBG;
        return true;
    }
    return false; // Profile not found
}

// Returns a pointer to the specified profile, or nullptr if not found
Profile* User::getProfile(const QString& profileName) {
    auto it = profiles.find(profileName);
    if (it != profiles.end()) {
        return &it->second;
    }
    return nullptr;
}

// Retrieves a list of all stored profile names
std::vector<QString> User::getAllProfileNames() const {
    std::vector<QString> profileNames;
    for (const auto& pair : profiles) {
        profileNames.push_back(pair.first);
    }
    return profileNames;
}

// Sets the active profile if it exists
bool User::setActiveProfile(const QString& profileName) {
    if (profiles.find(profileName) != profiles.end()) {
        activeProfile = profileName;
        return true;
    }
    return false; // Profile not found
}

// Returns a pointer to the currently active profile
Profile* User::getActiveProfile() const {
    auto it = profiles.find(activeProfile);
    if (it != profiles.end()) {
        return const_cast<Profile*>(&it->second); // const_cast used to return non-const pointer
    }
    return nullptr;
}

// Deletes the specified profile if it exists
bool User::deleteProfile(const QString& profileName){
    return profiles.erase(profileName) > 0;
}

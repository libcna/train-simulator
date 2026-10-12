#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace TrainSim::Config {

struct Settings
{
    float mouseSensitivity = 0.0022f;
    float masterVolume = 0.80f;
    float announcementVolume = 0.72f;
    float passengerVolume = 0.13f;
    float trainVolume = 1.0f;
    float cameraVibration = 0.65f;
    float fieldOfViewDegrees = 60.0f;
    int graphicsQuality = 2;
    float vegetationDistanceMetres = 750.0f;
    bool announcementCaptions = true;
    // Time of day and weather (ENV-138/139): the service-day start on the station clocks and a
    // Sim::WeatherMode index (0 clear, 1 cloudy, 2 overcast, 3 rain, 4 fog, 5 changing).
    int serviceStartMinutes = 9*60 + 17;
    int weatherMode = 0;

    [[nodiscard]] static Settings parse(std::string_view text);
    [[nodiscard]] static Settings parse(std::string_view text, Settings base);
    [[nodiscard]] static Settings load(const std::filesystem::path& path);
    [[nodiscard]] static Settings load(const std::filesystem::path& path, Settings base);
    [[nodiscard]] std::string serialize() const;
    void save(const std::filesystem::path& path) const;
};

// A command-line quality profile is a runtime QA override, not a preference update. Preserve the
// loaded value when another menu item is saved, unless the player explicitly changed quality too.
[[nodiscard]] Settings settingsForPersistence(
    Settings runtime, int loadedGraphicsQuality,
    bool launchQualityOverride, bool graphicsQualityAdjustedInMenu) noexcept;

} // namespace TrainSim::Config

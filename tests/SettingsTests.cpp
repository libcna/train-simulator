#include "TestHarness.hpp"
#include "trainsim/config/Settings.hpp"

#include <chrono>
#include <filesystem>

using TrainSim::Config::Settings;

TEST_CASE("settings parse and clamp")
{
    const Settings settings = Settings::parse(
        "mouse_sensitivity = 0.004\n"
        "master_volume = 3.0 # clamped\n"
        "camera_vibration = -1\n"
        "field_of_view_degrees = 82\n"
        "graphics_quality = 7\n"
        "vegetation_distance_metres = 5000\n"
        "announcement_captions = 0\n");
    REQUIRE(std::abs(settings.mouseSensitivity - 0.004f) < 1e-6f);
    REQUIRE(std::abs(settings.masterVolume - 1.0f) < 1e-6f);
    REQUIRE(std::abs(settings.cameraVibration - 0.0f) < 1e-6f);
    REQUIRE(std::abs(settings.fieldOfViewDegrees - 82.0f) < 1e-6f);
    REQUIRE(settings.graphicsQuality == 2);
    REQUIRE(std::abs(settings.vegetationDistanceMetres - 1'000.0f) < 1e-6f);
    REQUIRE(!settings.announcementCaptions);
}

TEST_CASE("settings overlay preserves unspecified values")
{
    Settings base;
    base.passengerVolume = 0.42f;
    const Settings settings = Settings::parse("train_volume=0.25\n", base);
    REQUIRE(std::abs(settings.passengerVolume - 0.42f) < 1e-6f);
    REQUIRE(std::abs(settings.trainVolume - 0.25f) < 1e-6f);
}

TEST_CASE("graphics quality resolves to three stable profiles")
{
    REQUIRE(Settings::parse("graphics_quality=-4\n").graphicsQuality == 0);
    REQUIRE(Settings::parse("graphics_quality=1\n").graphicsQuality == 1);
    REQUIRE(Settings::parse("graphics_quality=9\n").graphicsQuality == 2);
}

TEST_CASE("settings reject non-finite numeric values")
{
    bool rejected = false;
    try
    {
        static_cast<void>(Settings::parse("field_of_view_degrees=nan\n"));
    }
    catch (const std::runtime_error& error)
    {
        rejected = std::string(error.what()).find("line 1") != std::string::npos;
    }
    REQUIRE(rejected);
}

TEST_CASE("settings reject unknown keys instead of ignoring typos")
{
    bool rejected = false;
    try
    {
        static_cast<void>(Settings::parse("master_volum=0.4\n"));
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        rejected = message.find("master_volum") != std::string::npos
            && message.find("line 1") != std::string::npos;
    }
    REQUIRE(rejected);
}

TEST_CASE("settings serialize every value for a stable round trip")
{
    Settings original;
    original.mouseSensitivity = 0.0037f;
    original.masterVolume = 0.65f;
    original.announcementVolume = 0.44f;
    original.passengerVolume = 0.19f;
    original.trainVolume = 0.83f;
    original.cameraVibration = 0.20f;
    original.fieldOfViewDegrees = 78.0f;
    original.graphicsQuality = 1;
    original.vegetationDistanceMetres = 640.0f;
    original.announcementCaptions = false;
    original.serviceStartMinutes = 19*60 + 45;
    original.weatherMode = 5;

    const Settings restored = Settings::parse(original.serialize());
    REQUIRE(std::abs(restored.mouseSensitivity - original.mouseSensitivity) < 1e-6f);
    REQUIRE(std::abs(restored.masterVolume - original.masterVolume) < 1e-6f);
    REQUIRE(std::abs(restored.announcementVolume - original.announcementVolume) < 1e-6f);
    REQUIRE(std::abs(restored.passengerVolume - original.passengerVolume) < 1e-6f);
    REQUIRE(std::abs(restored.trainVolume - original.trainVolume) < 1e-6f);
    REQUIRE(std::abs(restored.cameraVibration - original.cameraVibration) < 1e-6f);
    REQUIRE(std::abs(restored.fieldOfViewDegrees - original.fieldOfViewDegrees) < 1e-6f);
    REQUIRE(restored.serviceStartMinutes == original.serviceStartMinutes);
    REQUIRE(restored.weatherMode == original.weatherMode);
    REQUIRE(restored.graphicsQuality == original.graphicsQuality);
    REQUIRE(std::abs(restored.vegetationDistanceMetres
                     - original.vegetationDistanceMetres) < 1e-6f);
    REQUIRE(restored.announcementCaptions == original.announcementCaptions);
}

TEST_CASE("settings save atomically and create the user configuration directory")
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("train-simulator-settings-" + std::to_string(unique));
    const std::filesystem::path file = root / "nested" / "train-simulator.cfg";
    Settings original;
    original.masterVolume = 0.37f;
    original.fieldOfViewDegrees = 85.0f;
    original.save(file);
    original.masterVolume = 0.44f;
    original.save(file);
    const Settings restored = Settings::load(file);
    const bool valid = std::abs(restored.masterVolume - original.masterVolume) < 1e-6f
        && std::abs(restored.fieldOfViewDegrees - original.fieldOfViewDegrees) < 1e-6f
        && !std::filesystem::exists(file.string() + ".tmp");
    std::filesystem::remove_all(root);
    REQUIRE(valid);
}

TEST_CASE("temporary launch quality does not leak into unrelated preference saves")
{
    Settings runtime;
    runtime.graphicsQuality = 0;
    runtime.masterVolume = 0.37f;

    const Settings unrelatedSave = TrainSim::Config::settingsForPersistence(
        runtime,2,true,false);
    REQUIRE(unrelatedSave.graphicsQuality == 2);
    REQUIRE(std::abs(unrelatedSave.masterVolume-runtime.masterVolume) < 1e-6f);

    const Settings explicitQualitySave = TrainSim::Config::settingsForPersistence(
        runtime,2,true,true);
    REQUIRE(explicitQualitySave.graphicsQuality == 0);

    const Settings ordinaryLaunchSave = TrainSim::Config::settingsForPersistence(
        runtime,2,false,false);
    REQUIRE(ordinaryLaunchSave.graphicsQuality == 0);
}

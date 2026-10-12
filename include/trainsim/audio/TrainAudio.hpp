#pragma once

#include "trainsim/sim/Environment.hpp"
#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"

#include <filesystem>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace TrainSim::World {
class StreamingWorld;
}

namespace TrainSim::Audio {

struct TrainAudioSettings
{
    float masterVolume = 0.8f;
    float announcementVolume = 0.72f;
    float passengerVolume = 0.13f;
    float trainVolume = 1.0f;
};

// Pure route-aware scripts keep captions, direction changes, and the generated audio vocabulary
// testable without constructing an audio device. Runtime playback consumes these exact strings.
[[nodiscard]] std::string welcomeAnnouncementCaption(const Sim::Route& route, int direction);
[[nodiscard]] std::string stationAnnouncementCaption(
    const Sim::Route& route, std::size_t stationIndex, bool alternate);
[[nodiscard]] std::string terminalAnnouncementCaption(
    const Sim::Route& route, std::size_t terminalStationIndex, int onwardDirection);
[[nodiscard]] float advanceAnnouncementDuck(
    float currentAmount, bool announcementPlaying, double seconds) noexcept;

class TrainAudio
{
public:
    explicit TrainAudio(const std::filesystem::path& assetDirectory,
                        TrainAudioSettings settings = {},
                        bool passengerDoorsInitiallyOpen = true,
                        bool toiletDoorInitiallyOpen = false);
    ~TrainAudio();

    TrainAudio(const TrainAudio&) = delete;
    TrainAudio& operator=(const TrainAudio&) = delete;

    void update(double realSeconds, const Sim::Route& route,
                const Sim::RouteSimulation& service, const Sim::PassengerSystem& passengers,
                const World::StreamingWorld& world,
                float playerX, float playerZ, float playerYaw, bool playerWalking,
                bool toiletDoorOpen, bool inTunnel, bool onBridge);
    void setSettings(TrainAudioSettings settings);
    // Rain ambience (ENV-139) follows the weather mode and simulated service time.
    void setWeather(Sim::WeatherMode mode) noexcept;
    [[nodiscard]] float rainLevel() const noexcept;
    void setPaused(bool paused) noexcept;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool paused() const noexcept;
    [[nodiscard]] std::size_t playingInstanceCount() const noexcept;
    [[nodiscard]] std::size_t pausedInstanceCount() const noexcept;
    [[nodiscard]] std::string_view currentAnnouncementCaption() const noexcept;
    [[nodiscard]] std::string_view lastAnnouncementCaption() const noexcept;
    [[nodiscard]] std::string_view queuedAnnouncementCaption() const noexcept;
    [[nodiscard]] std::size_t announcementChimeEventCount() const noexcept;
    [[nodiscard]] std::size_t announcementEventCount() const noexcept;
    [[nodiscard]] std::size_t terminalAnnouncementEventCount() const noexcept;
    [[nodiscard]] std::size_t passengerMurmurEventCount() const noexcept;
    [[nodiscard]] std::size_t passingTrainEventCount() const noexcept;
    [[nodiscard]] std::size_t doorTransitionEventCount() const noexcept;
    [[nodiscard]] std::size_t toiletDoorEventCount() const noexcept;
    [[nodiscard]] std::size_t crossingWarningEventCount() const noexcept;
    [[nodiscard]] std::size_t carpetFootstepEventCount() const noexcept;
    [[nodiscard]] std::size_t nonCarpetedFootstepEventCount() const noexcept;
    [[nodiscard]] bool tunnelAcousticsActive() const noexcept;
    [[nodiscard]] bool bridgeAcousticsActive() const noexcept;
    [[nodiscard]] bool interiorAmbienceActive() const noexcept;
    [[nodiscard]] float tractionEffort() const noexcept;
    [[nodiscard]] float brakingEffort() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace TrainSim::Audio

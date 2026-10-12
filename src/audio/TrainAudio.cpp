#include "trainsim/audio/TrainAudio.hpp"

#include "trainsim/sim/Environment.hpp"
#include "trainsim/sim/TrainInterior.hpp"
#include "trainsim/world/OtherTrainConsist.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include "Microsoft/Xna/Framework/Audio/AudioEmitter.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioListener.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace TrainSim::Audio {

namespace XnaAudio = Microsoft::Xna::Framework::Audio;
using Microsoft::Xna::Framework::Vector3;

namespace {
constexpr double kAnnouncementChimeSeconds = 0.92;
}

std::string welcomeAnnouncementCaption(const Sim::Route& route, int direction)
{
    const std::string& destination = direction >= 0 ? route.stations().back().name
                                                     : route.stations().front().name;
    return "WELCOME ABOARD THE NORTHERN VALLEY EXPRESS.\n"
           "THIS TRAIN IS BOUND FOR " + destination + ".";
}

std::string stationAnnouncementCaption(const Sim::Route& route, std::size_t stationIndex,
                                       bool alternate)
{
    if (stationIndex >= route.stations().size())
        throw std::out_of_range("Announcement station index is outside the route");
    const std::string& stationName = route.stations()[stationIndex].name;
    if (alternate)
        return "WE ARE NOW APPROACHING " + stationName + ".\n"
               "PLEASE MAKE SURE YOU HAVE ALL YOUR BELONGINGS\n"
               "AND MIND THE GAP WHEN LEAVING THE TRAIN.";
    return "THE NEXT STATION IS " + stationName + ".\n"
           "WE WILL SHORTLY BE ARRIVING AT " + stationName + ".\n"
           "PLEASE MIND THE GAP BETWEEN THE TRAIN AND THE PLATFORM.";
}

std::string terminalAnnouncementCaption(const Sim::Route& route,
                                        std::size_t terminalStationIndex,
                                        int onwardDirection)
{
    if (terminalStationIndex >= route.stations().size())
        throw std::out_of_range("Announcement terminal index is outside the route");
    const std::string& terminal = route.stations()[terminalStationIndex].name;
    const std::string& destination = onwardDirection < 0 ? route.stations().front().name
                                                         : route.stations().back().name;
    return "THIS IS " + terminal + ", THE FINAL STOP.\n"
           "PLEASE REMEMBER TO TAKE ALL YOUR BELONGINGS WITH YOU.\n"
           "THIS SERVICE WILL SHORTLY CONTINUE TO " + destination + ".";
}

float advanceAnnouncementDuck(float currentAmount, bool announcementPlaying,
                              double seconds) noexcept
{
    const float current = std::isfinite(currentAmount)
        ? std::clamp(currentAmount,0.0f,1.0f) : 0.0f;
    if (!std::isfinite(seconds) || seconds <= 0.0) return current;
    const float rate = announcementPlaying ? 2.4f : -1.25f;
    return std::clamp(current+rate*static_cast<float>(seconds),0.0f,1.0f);
}

class TrainAudio::Impl
{
public:
    explicit Impl(const std::filesystem::path& assetDirectory, TrainAudioSettings settings,
                  bool passengerDoorsInitiallyOpen, bool toiletDoorInitiallyOpen)
        : settings_(settings), previousDoorsOpen_(passengerDoorsInitiallyOpen),
          previousToiletDoorOpen_(toiletDoorInitiallyOpen)
    {
        try
        {
            XnaAudio::SoundEffect::setMasterVolumeProperty(settings_.masterVolume);
            rollingEffect_ = load(assetDirectory / "rolling.wav");
            tractionEffect_ = load(assetDirectory / "traction.wav");
            windEffect_ = load(assetDirectory / "wind.wav");
            tunnelEffect_ = load(assetDirectory / "tunnel_rumble.wav");
            bridgeEffect_ = load(assetDirectory / "bridge_rumble.wav");
            brakingEffect_ = load(assetDirectory / "braking.wav");
            curveSquealEffect_ = load(assetDirectory / "curve_squeal.wav");
            railTextureEffect_ = load(assetDirectory / "rail_texture.wav");
            gangwayRattleEffect_ = load(assetDirectory / "gangway_rattle.wav");
            gangwayDoorEffect_ = load(assetDirectory / "gangway_door.wav");
            toiletDoorEffect_ = load(assetDirectory / "toilet_door.wav");
            passingTrainEffect_ = load(assetDirectory / "passing_train.wav");
            crossingWarningEffect_ = load(assetDirectory / "crossing_warning.wav");
            // Optional so asset directories from before ENV-139 still play everything else.
            if (std::filesystem::exists(assetDirectory / "rain.wav"))
                rainEffect_ = load(assetDirectory / "rain.wav");
            for (const char* name : {"footstep_carpet_01.wav", "footstep_carpet_02.wav"})
                carpetFootstepEffects_.push_back(load(assetDirectory / name));
            for (const char* name : {"footstep_vestibule_01.wav", "footstep_vestibule_02.wav"})
                nonCarpetedFootstepEffects_.push_back(load(assetDirectory / name));
            for (const char* name : {"passenger_murmur.wav",
                                     "passenger_murmur_01.wav",
                                     "passenger_murmur_02.wav",
                                     "passenger_murmur_03.wav"})
                murmurEffects_.push_back(load(assetDirectory / name));
            nearbyPassengerScratch_.reserve(48);
            talkingPassengerScratch_.reserve(48);
            stationAmbienceEffect_ = load(assetDirectory / "station_ambience.wav");
            interiorHvacEffect_ = load(assetDirectory / "interior_hvac.wav");
            announcementChimeEffect_ = load(assetDirectory / "announcement_chime.wav");
            doorWarningEffect_ = load(assetDirectory / "door_warning.wav");
            doorOpenEffect_ = load(assetDirectory / "door_open.wav");
            doorCloseEffect_ = load(assetDirectory / "door_close.wav");
            welcomeWesthavenEffect_ = load(assetDirectory / "announcement_welcome.wav");
            welcomeAlderwickEffect_ = load(assetDirectory / "announcement_welcome_alderwick.wav");
            reverseAlderwickEffect_ = load(
                assetDirectory / "announcement_reverse_to_alderwick.wav");
            reverseWesthavenEffect_ = load(
                assetDirectory / "announcement_reverse_to_westhaven.wav");
            for (int index = 0; index < 12; ++index)
            {
                std::string suffix = index < 10 ? "0" + std::to_string(index) : std::to_string(index);
                stationEffects_.push_back(load(assetDirectory / ("announcement_station_" + suffix + ".wav")));
                stationAlternateEffects_.push_back(load(
                    assetDirectory / ("announcement_station_" + suffix + "_alternate.wav")));
            }

            rolling_ = makeLoop(*rollingEffect_, 0.0f);
            traction_ = makeLoop(*tractionEffect_, 0.0f);
            wind_ = makeLoop(*windEffect_, 0.0f);
            tunnel_ = makeLoop(*tunnelEffect_, 0.0f);
            bridge_ = makeLoop(*bridgeEffect_, 0.0f);
            braking_ = makeLoop(*brakingEffect_, 0.0f);
            curveSqueal_ = makeLoop(*curveSquealEffect_, 0.0f);
            railTexture_ = makeLoop(*railTextureEffect_, 0.0f);
            gangwayRattle_ = makeSpatialLoop(*gangwayRattleEffect_);
            crossingWarning_ = makeSpatialLoop(*crossingWarningEffect_);
            stationAmbience_ = makeLoop(*stationAmbienceEffect_, 0.0f);
            interiorHvac_ = makeLoop(*interiorHvacEffect_, 0.0f);
            if (rainEffect_) rain_ = makeLoop(*rainEffect_, 0.0f);
            available_ = true;
        }
        catch (const std::exception& error)
        {
            std::cerr << "Train audio unavailable: " << error.what() << '\n';
            stopAll();
        }
    }

    ~Impl() { stopAll(); }

    [[nodiscard]] bool available() const noexcept { return available_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] std::size_t playingInstanceCount() const noexcept
    {
        return instanceCount(XnaAudio::SoundState::Playing);
    }
    [[nodiscard]] std::size_t pausedInstanceCount() const noexcept
    {
        return instanceCount(XnaAudio::SoundState::Paused);
    }
    [[nodiscard]] std::size_t announcementEventCount() const noexcept
    {
        return announcementEventCount_;
    }
    [[nodiscard]] std::size_t announcementChimeEventCount() const noexcept
    {
        return announcementChimeEventCount_;
    }
    [[nodiscard]] std::size_t terminalAnnouncementEventCount() const noexcept
    {
        return terminalAnnouncementEventCount_;
    }
    [[nodiscard]] std::size_t passengerMurmurEventCount() const noexcept
    {
        return passengerMurmurEventCount_;
    }
    [[nodiscard]] std::size_t passingTrainEventCount() const noexcept
    {
        return passingTrainEventCount_;
    }
    [[nodiscard]] std::size_t doorTransitionEventCount() const noexcept
    {
        return doorTransitionEventCount_;
    }
    [[nodiscard]] std::size_t toiletDoorEventCount() const noexcept
    {
        return toiletDoorEventCount_;
    }
    [[nodiscard]] std::size_t crossingWarningEventCount() const noexcept
    {
        return crossingWarningEventCount_;
    }
    [[nodiscard]] std::size_t carpetFootstepEventCount() const noexcept
    {
        return carpetFootstepEventCount_;
    }
    [[nodiscard]] std::size_t nonCarpetedFootstepEventCount() const noexcept
    {
        return nonCarpetedFootstepEventCount_;
    }
    [[nodiscard]] bool tunnelAcousticsActive() const noexcept
    {
        return tunnelMix_ > 0.01f;
    }
    [[nodiscard]] bool bridgeAcousticsActive() const noexcept
    {
        return bridgeMix_ > 0.01f;
    }
    [[nodiscard]] bool interiorAmbienceActive() const noexcept
    {
        try
        {
            return interiorHvac_
                && interiorHvac_->getStateProperty() == XnaAudio::SoundState::Playing;
        }
        catch (...)
        {
            return false;
        }
    }
    [[nodiscard]] float tractionEffort() const noexcept { return tractionEffort_; }
    [[nodiscard]] float brakingEffort() const noexcept { return brakingEffort_; }

    [[nodiscard]] std::string_view currentAnnouncementCaption() const noexcept
    {
        try
        {
            return announcementStopped() ? std::string_view{} : announcementCaption_;
        }
        catch (...)
        {
            return {};
        }
    }

    [[nodiscard]] std::string_view lastAnnouncementCaption() const noexcept
    {
        return lastAnnouncementCaption_;
    }

    [[nodiscard]] std::string_view queuedAnnouncementCaption() const noexcept
    {
        return pendingAnnouncement_.has_value()
            ? std::string_view(pendingAnnouncement_->caption) : std::string_view{};
    }

    void setWeather(Sim::WeatherMode mode) noexcept { weatherMode_ = mode; }
    [[nodiscard]] float rainLevel() const noexcept { return rainMix_; }

    void setSettings(TrainAudioSettings settings)
    {
        settings.masterVolume = std::clamp(settings.masterVolume, 0.0f, 1.0f);
        settings.announcementVolume = std::clamp(settings.announcementVolume, 0.0f, 1.0f);
        settings.passengerVolume = std::clamp(settings.passengerVolume, 0.0f, 1.0f);
        settings.trainVolume = std::clamp(settings.trainVolume, 0.0f, 1.0f);
        settings_ = settings;
        XnaAudio::SoundEffect::setMasterVolumeProperty(settings_.masterVolume);
        if (announcement_) announcement_->setVolumeProperty(settings_.announcementVolume);
        if (announcementChime_)
            announcementChime_->setVolumeProperty(0.58f * settings_.announcementVolume);
        if (murmur_)
            murmur_->setVolumeProperty(
                settings_.passengerVolume*(1.0f-0.82f*announcementDuck_));
        if (doorInstance_) doorInstance_->setVolumeProperty(0.55f * settings_.trainVolume);
        if (doorWarningInstance_)
            doorWarningInstance_->setVolumeProperty(0.58f * settings_.trainVolume);
        if (gangwayDoor_)
            gangwayDoor_->setVolumeProperty(0.24f * settings_.trainVolume);
        if (toiletDoor_)
            toiletDoor_->setVolumeProperty(0.28f * settings_.trainVolume);
        if (passingTrain_)
            passingTrain_->setVolumeProperty(0.34f * settings_.trainVolume);
        if (crossingWarning_)
            crossingWarning_->setVolumeProperty(
                crossingWarningLevel_ * settings_.trainVolume
                * (1.0f-0.18f*announcementDuck_));
        if (footstep_)
            footstep_->setVolumeProperty(footstepBaseVolume_ * settings_.trainVolume);
        applyTrainMix();
    }

    void setPaused(bool paused) noexcept
    {
        if (paused_ == paused) return;
        if (!available_)
        {
            paused_ = paused;
            return;
        }
        try
        {
            for (XnaAudio::SoundEffectInstance* instance : instances())
            {
                if (instance == nullptr) continue;
                const XnaAudio::SoundState state = instance->getStateProperty();
                if (paused && state == XnaAudio::SoundState::Playing)
                    instance->Pause();
                else if (!paused && state == XnaAudio::SoundState::Paused)
                    instance->Resume();
            }
            paused_ = paused;
        }
        catch (const std::exception& error)
        {
            std::cerr << "Train audio disabled after pause transition error: "
                      << error.what() << '\n';
            available_ = false;
            paused_ = paused;
            stopAll();
        }
        catch (...)
        {
            std::cerr << "Train audio disabled after unknown pause transition error\n";
            available_ = false;
            paused_ = paused;
            stopAll();
        }
    }

    void update(double realSeconds, const Sim::Route& route,
                const Sim::RouteSimulation& service, const Sim::PassengerSystem& passengers,
                const World::StreamingWorld& world,
                float playerX, float playerZ, float playerYaw, bool playerWalking,
                bool toiletDoorOpen, bool inTunnel, bool onBridge)
    {
        if (!available_ || paused_) return;
        try
        {
            const auto& state = service.state();
            const float speedRatio = std::clamp(static_cast<float>(state.speedMetresPerSecond / 44.44),
                                                0.0f, 1.0f);
            tunnelMix_ += (inTunnel ? 1.0f : -1.0f)
                        * static_cast<float>(realSeconds) * 0.9f;
            tunnelMix_ = std::clamp(tunnelMix_, 0.0f, 1.0f);
            bridgeMix_ += (onBridge ? 1.0f : -1.0f)
                        * static_cast<float>(realSeconds) * 1.6f;
            bridgeMix_ = std::clamp(bridgeMix_, 0.0f, 1.0f);
            // Rain on the roof and glazing, muted in tunnels and following weather changes.
            const float rainTarget = Sim::weatherConditions(
                weatherMode_,state.serviceElapsedSeconds).rain*(inTunnel ? 0.0f : 1.0f);
            rainMix_ += std::clamp(rainTarget-rainMix_,-static_cast<float>(realSeconds)*0.5f,
                                   static_cast<float>(realSeconds)*0.5f);
            const bool platformOpen = state.phase == Sim::ServicePhase::Dwelling && state.doorsOpen;
            stationMix_ += (platformOpen ? 1.0f : -1.0f)
                         * static_cast<float>(realSeconds) * 0.65f;
            stationMix_ = std::clamp(stationMix_, 0.0f, 1.0f);

            speedRatio_ = speedRatio;
            const float targetTraction = std::clamp(static_cast<float>(
                state.longitudinalAccelerationMetresPerSecondSquared/0.62),0.0f,1.0f);
            const float targetBraking = std::clamp(static_cast<float>(
                -state.longitudinalAccelerationMetresPerSecondSquared/0.78),0.0f,1.0f);
            const float effortStep = std::clamp(
                static_cast<float>(realSeconds)*3.2f,0.0f,1.0f);
            tractionEffort_ += std::clamp(
                targetTraction-tractionEffort_,-effortStep,effortStep);
            brakingEffort_ += std::clamp(
                targetBraking-brakingEffort_,-effortStep,effortStep);
            const Sim::RoutePose routePose = route.sample(state.routeDistanceMetres);
            curveAmount_ = std::clamp(
                static_cast<float>(std::abs(routePose.curvature) * 22'000.0 - 0.18), 0.0f, 1.0f);
            announcementDuck_ = advanceAnnouncementDuck(
                announcementDuck_,!announcementSequenceIdle(),realSeconds);
            advancePendingAnnouncement(realSeconds);
            const int gangwayIndex = std::clamp(
                static_cast<int>(std::lround(
                    (playerZ - static_cast<float>(Sim::TrainInterior::FirstGangwayZ))
                    / static_cast<float>(Sim::TrainInterior::CarLength))),
                0, Sim::TrainInterior::GangwayCount - 1);
            gangwayZ_ = static_cast<float>(Sim::TrainInterior::gangwayZ(gangwayIndex));
            gangwayProximity_ = std::clamp(1.0f - std::abs(playerZ - gangwayZ_) / 8.0f,
                                           0.0f, 1.0f);
            applyTrainMix();

            rolling_->setPitchProperty(-0.34f + speedRatio * 0.48f);
            wind_->setPitchProperty(-0.12f + speedRatio * 0.20f);
            traction_->setPitchProperty(-0.28f + speedRatio * 0.58f);
            tunnel_->setPitchProperty(-0.18f + speedRatio * 0.20f);
            bridge_->setPitchProperty(-0.25f + speedRatio * 0.22f);
            braking_->setPitchProperty(-0.12f + speedRatio * 0.22f);
            curveSqueal_->setPitchProperty(-0.08f + speedRatio * 0.18f);
            railTexture_->setPitchProperty(-0.42f + speedRatio * 0.55f);
            gangwayRattle_->setPitchProperty(-0.20f + speedRatio * 0.32f);
            positionGangway(playerX, playerZ, playerYaw);
            updateGangwayDoors(passengers, playerX, playerZ, playerYaw, gangwayIndex);
            updateToiletDoor(toiletDoorOpen, playerX, playerZ, playerYaw);
            updatePassingTrain(realSeconds,route,service,world,playerX,playerZ,playerYaw);
            updateLevelCrossing(route,service,world,playerX,playerZ,playerYaw);
            updateFootsteps(realSeconds, playerWalking, playerX, playerZ);
            updateMurmur(realSeconds, passengers, playerX, playerZ, playerYaw);

            elapsed_ += realSeconds;
            // An imminent stop is operationally more important than the generic welcome. This
            // also prevents a resumed/diagnostic journey near a station from delaying the useful
            // arrival message by the full welcome recording.
            if (service.distanceToNextStation() < 1'650.0
                && state.speedMetresPerSecond > 7.0
                && announcedStation_ != state.nextStation
                && announcementSequenceIdle())
            {
                if (state.nextStation < stationEffects_.size())
                {
                    const bool alternate = (state.completedLegs + state.nextStation) % 2U != 0U;
                    if (queueAnnouncement(alternate
                            ? stationAlternateEffects_[state.nextStation].get()
                            : stationEffects_[state.nextStation].get(),
                            stationAnnouncementCaption(route,state.nextStation,alternate)))
                        announcedStation_ = state.nextStation;
                }
            }
            if (!welcomePlayed_ && elapsed_ > 0.10)
            {
                welcomePlayed_ = true;
                const bool towardWesthaven = state.direction >= 0;
                (void)queueAnnouncement(towardWesthaven ? welcomeWesthavenEffect_.get()
                                                        : welcomeAlderwickEffect_.get(),
                    welcomeAnnouncementCaption(route,state.direction));
            }

            if (pendingDoorClose_ > 0.0)
            {
                pendingDoorClose_ -= realSeconds;
                if (pendingDoorClose_ <= 0.0)
                    playOneShot(*doorCloseEffect_, doorInstance_, 0.55f * settings_.trainVolume);
            }

            if (state.doorsOpen != previousDoorsOpen_)
            {
                ++doorTransitionEventCount_;
                if (state.doorsOpen)
                {
                    playOneShot(*doorOpenEffect_, doorInstance_, 0.55f * settings_.trainVolume);
                    const bool terminal = state.currentStation == 0
                                       || state.currentStation + 1 == route.stations().size();
                    if (terminal && state.completedLegs > 0)
                        pendingTerminalDirection_ = state.direction;
                }
                else
                {
                    playOneShot(*doorWarningEffect_, doorWarningInstance_,
                                0.58f * settings_.trainVolume);
                    pendingDoorClose_ = 1.9;
                }
                previousDoorsOpen_ = state.doorsOpen;
            }

            if (state.completedLegs != previousCompletedLegs_)
            {
                announcedStation_.reset();
                previousCompletedLegs_ = state.completedLegs;
            }

            if (pendingTerminalDirection_.has_value() && announcementSequenceIdle())
            {
                const int direction = *pendingTerminalDirection_;
                const bool towardAlderwick = direction < 0;
                if (queueAnnouncement(towardAlderwick ? reverseAlderwickEffect_.get()
                                                      : reverseWesthavenEffect_.get(),
                        terminalAnnouncementCaption(route,state.currentStation,direction),true))
                    pendingTerminalDirection_.reset();
            }

        }
        catch (const std::exception& error)
        {
            std::cerr << "Train audio disabled after playback error: " << error.what() << '\n';
            available_ = false;
            stopAll();
        }
    }

private:
    [[nodiscard]] std::array<XnaAudio::SoundEffectInstance*,22> instances() const noexcept
    {
        return {rolling_.get(), traction_.get(), wind_.get(), tunnel_.get(), bridge_.get(),
                braking_.get(), curveSqueal_.get(), railTexture_.get(), gangwayRattle_.get(),
                gangwayDoor_.get(), toiletDoor_.get(), passingTrain_.get(),
                crossingWarning_.get(), footstep_.get(), murmur_.get(),
                stationAmbience_.get(), interiorHvac_.get(), doorInstance_.get(),
                doorWarningInstance_.get(), announcementChime_.get(), announcement_.get(),
                rain_.get()};
    }

    [[nodiscard]] std::size_t instanceCount(XnaAudio::SoundState wanted) const noexcept
    {
        std::size_t count = 0;
        try
        {
            for (const XnaAudio::SoundEffectInstance* instance : instances())
                if (instance != nullptr && instance->getStateProperty() == wanted) ++count;
        }
        catch (...)
        {
            // Diagnostics must stay safe during teardown or after an audio-device failure.
            return 0;
        }
        return count;
    }

    void applyTrainMix()
    {
        if (!rolling_) return;
        const float mix = settings_.trainVolume;
        const float bed = mix*(1.0f-0.30f*announcementDuck_);
        rolling_->setVolumeProperty((0.04f + speedRatio_ * 0.31f) * bed);
        wind_->setVolumeProperty(speedRatio_ * speedRatio_ * 0.18f * bed);
        traction_->setVolumeProperty(
            (0.025f + tractionEffort_*(0.055f+speedRatio_*0.19f)) * bed);
        tunnel_->setVolumeProperty(tunnelMix_ * (0.10f + speedRatio_ * 0.30f) * bed);
        bridge_->setVolumeProperty(bridgeMix_ * (0.08f + speedRatio_ * 0.22f) * bed);
        braking_->setVolumeProperty(brakingEffort_ * speedRatio_ * 0.18f * bed);
        curveSqueal_->setVolumeProperty(curveAmount_ * speedRatio_ * 0.11f * bed);
        railTexture_->setVolumeProperty(speedRatio_ * 0.11f * bed);
        gangwayRattle_->setVolumeProperty(
            gangwayProximity_ * speedRatio_ * (0.035f + speedRatio_ * 0.055f) * bed);
        stationAmbience_->setVolumeProperty(
            stationMix_ * 0.12f * mix*(1.0f-0.55f*announcementDuck_));
        interiorHvac_->setVolumeProperty(
            (0.075f+0.025f*(1.0f-stationMix_))*bed);
        // Louder through open platform doors than through the closed bodyshell.
        if (rain_) rain_->setVolumeProperty(rainMix_*(0.09f+0.10f*stationMix_)*mix);
    }

    void positionGangway(float playerX, float playerZ, float playerYaw)
    {
        if (!gangwayRattle_) return;
        XnaAudio::AudioListener listener;
        listener.setPositionProperty(Vector3(playerX, 1.55f, playerZ));
        listener.setForwardProperty(Vector3(std::sin(playerYaw), 0.0f, -std::cos(playerYaw)));
        XnaAudio::AudioEmitter emitter;
        emitter.setPositionProperty(Vector3(0.0f, 1.0f, gangwayZ_));
        gangwayRattle_->Apply3D(listener, emitter);
    }

    void positionGangwayDoor(float playerX, float playerZ, float playerYaw)
    {
        if (!gangwayDoor_) return;
        XnaAudio::AudioListener listener;
        listener.setPositionProperty(Vector3(playerX, 1.55f, playerZ));
        listener.setForwardProperty(Vector3(std::sin(playerYaw), 0.0f, -std::cos(playerYaw)));
        XnaAudio::AudioEmitter emitter;
        emitter.setPositionProperty(Vector3(0.0f, 1.0f, gangwayDoorZ_));
        gangwayDoor_->Apply3D(listener, emitter);
    }

    void updateGangwayDoors(const Sim::PassengerSystem& passengers, float playerX, float playerZ,
                            float playerYaw, int nearestGangway)
    {
        for (int index = 0; index < Sim::TrainInterior::GangwayCount; ++index)
        {
            bool occupied = Sim::TrainInterior::gangwaySensorContains(index, playerZ);
            if (!occupied)
                occupied = std::any_of(
                    passengers.passengers().begin(), passengers.passengers().end(),
                    [index](const Sim::Passenger& passenger) {
                        return passenger.role != Sim::PassengerRole::Driver
                            && !passenger.seat.has_value()
                            && Sim::TrainInterior::gangwaySensorContains(index, passenger.z);
                    });

            bool& previous = previousGangwaySensors_[static_cast<std::size_t>(index)];
            if (occupied != previous && index == nearestGangway
                && std::abs(playerZ - static_cast<float>(Sim::TrainInterior::gangwayZ(index))) < 9.0f)
            {
                stop(gangwayDoor_);
                gangwayDoor_ = std::make_unique<XnaAudio::SoundEffectInstance>(
                    gangwayDoorEffect_->CreateInstance());
                gangwayDoor_->setVolumeProperty(0.24f * settings_.trainVolume);
                gangwayDoor_->setPitchProperty(occupied ? 0.08f : -0.10f);
                gangwayDoorZ_ = static_cast<float>(Sim::TrainInterior::gangwayZ(index));
                positionGangwayDoor(playerX, playerZ, playerYaw);
                gangwayDoor_->Play();
            }
            previous = occupied;
        }
        positionGangwayDoor(playerX, playerZ, playerYaw);
    }

    void positionToiletDoor(float playerX, float playerZ, float playerYaw)
    {
        if (!toiletDoor_) return;
        XnaAudio::AudioListener listener;
        listener.setPositionProperty(Vector3(playerX, 1.55f, playerZ));
        listener.setForwardProperty(Vector3(std::sin(playerYaw), 0.0f, -std::cos(playerYaw)));
        XnaAudio::AudioEmitter emitter;
        emitter.setPositionProperty(Vector3(0.37f, 1.15f, 4.07f));
        toiletDoor_->Apply3D(listener, emitter);
    }

    void updateToiletDoor(bool open, float playerX, float playerZ, float playerYaw)
    {
        if (open != previousToiletDoorOpen_)
        {
            stop(toiletDoor_);
            toiletDoor_ = std::make_unique<XnaAudio::SoundEffectInstance>(
                toiletDoorEffect_->CreateInstance());
            toiletDoor_->setVolumeProperty(0.28f * settings_.trainVolume);
            toiletDoor_->setPitchProperty(open ? 0.05f : -0.07f);
            positionToiletDoor(playerX, playerZ, playerYaw);
            toiletDoor_->Play();
            ++toiletDoorEventCount_;
            previousToiletDoorOpen_ = open;
        }
        positionToiletDoor(playerX, playerZ, playerYaw);
    }

    void updateFootsteps(double realSeconds, bool playerWalking, float playerX, float playerZ)
    {
        footstepCooldown_ = std::max(0.0, footstepCooldown_ - realSeconds);
        if (!playerWalking || footstepCooldown_ > 0.0) return;
        const bool nonCarpeted = Sim::TrainInterior::nonCarpetedFloorAt(playerX,playerZ);
        const auto& effects = nonCarpeted ? nonCarpetedFootstepEffects_
                                          : carpetFootstepEffects_;
        if (effects.empty()) return;
        stop(footstep_);
        const std::size_t variant = footstepSequence_++ % effects.size();
        footstep_ = std::make_unique<XnaAudio::SoundEffectInstance>(
            effects[variant]->CreateInstance());
        footstepBaseVolume_ = nonCarpeted ? 0.105f : 0.13f;
        footstep_->setVolumeProperty(footstepBaseVolume_ * settings_.trainVolume);
        footstep_->setPitchProperty(variant == 0 ? -0.035f : 0.025f);
        footstep_->Play();
        if (nonCarpeted)
            ++nonCarpetedFootstepEventCount_;
        else
            ++carpetFootstepEventCount_;
        footstepCooldown_ = 0.46;
    }

    void updatePassingTrain(double realSeconds, const Sim::Route& route,
                            const Sim::RouteSimulation& service,
                            const World::StreamingWorld& world,
                            float playerX, float playerZ, float playerYaw)
    {
        passingTrainCooldown_ = std::max(0.0,passingTrainCooldown_-realSeconds);
        if (passingTrain_ && passingTrain_->getStateProperty() == XnaAudio::SoundState::Stopped)
            stop(passingTrain_);
        if (passingTrainCooldown_ > 0.0 || passingTrainEffect_ == nullptr) return;

        for (const World::WorldFeature& feature : world.otherTrains())
        {
            if (!World::movingOtherTrainVisible(
                    feature,elapsed_,route.lengthMetres())) continue;
            const double routeDistance = World::movingOtherTrainDistance(
                feature,elapsed_,route.lengthMetres());
            const Sim::RelativeRoutePose relative = route.relativePose(
                service.state().routeDistanceMetres,routeDistance);
            const double lateral = route.parallelTrackLateralMetres(routeDistance);
            const double cosine = std::cos(relative.headingRadians);
            const double sine = std::sin(relative.headingRadians);
            const float emitterX = static_cast<float>(relative.x+lateral*cosine);
            const float centreZ = static_cast<float>(relative.z-lateral*sine);
            // The source represents the whole consist rather than a point. Its half-length
            // (28.5 m for the regional unit, about 100 m for a freight) starts the pressure
            // envelope as the leading vehicle reaches the listener and keeps the event stable
            // across a long offscreen frame without making a distant service sound close.
            // The emitter follows the vehicle nearest the listener along the consist.
            const World::OtherTrainType type = World::otherTrainType(feature.variant);
            const float halfLength = static_cast<float>(
                World::otherTrainConsist(type).lengthMetres()*0.5);
            if (std::abs(centreZ-playerZ) > halfLength) continue;
            const float emitterZ = std::clamp(playerZ,centreZ-halfLength,centreZ+halfLength);

            passingTrain_ = std::make_unique<XnaAudio::SoundEffectInstance>(
                passingTrainEffect_->CreateInstance());
            // Freight runs heavier and lower; the intercity is quicker and brighter.
            const float typeVolume = type == World::OtherTrainType::Freight ? 0.42f : 0.34f;
            const float typePitch = type == World::OtherTrainType::Freight ? -0.16f
                                  : type == World::OtherTrainType::Intercity ? 0.05f
                                  : type == World::OtherTrainType::DieselUnit ? -0.08f : 0.0f;
            passingTrain_->setVolumeProperty(typeVolume*settings_.trainVolume);
            passingTrain_->setPitchProperty(std::clamp(-0.06f + typePitch
                + static_cast<float>(World::otherTrainPhaseVariant(feature.variant)%3U)*0.045f,
                -1.0f,1.0f));
            XnaAudio::AudioListener listener;
            listener.setPositionProperty(Vector3(playerX,1.55f,playerZ));
            listener.setForwardProperty(Vector3(
                std::sin(playerYaw),0.0f,-std::cos(playerYaw)));
            XnaAudio::AudioEmitter emitter;
            emitter.setPositionProperty(Vector3(emitterX,1.15f,emitterZ));
            passingTrain_->Apply3D(listener,emitter);
            passingTrain_->Play();
            passingTrainCooldown_ = 8.0;
            ++passingTrainEventCount_;
            return;
        }
    }

    void updateLevelCrossing(const Sim::Route& route,
                             const Sim::RouteSimulation& service,
                             const World::StreamingWorld& world,
                             float playerX, float playerZ, float playerYaw)
    {
        if (!crossingWarning_) return;

        float bestLevel = 0.0f;
        float bestX = 0.0f;
        float bestZ = 0.0f;
        std::uint32_t bestVariant = 0;
        for (const World::Chunk& chunk : world.chunks())
            for (const World::WorldFeature& feature : chunk.features)
            {
                if (feature.kind != World::FeatureKind::LevelCrossing) continue;
                const double closure = world.requiredLevelCrossingClosure(
                    feature.routeDistance,service.state().routeDistanceMetres,elapsed_);
                if (closure <= 0.0) continue;

                const Sim::RelativeRoutePose relative = route.relativePose(
                    service.state().routeDistanceMetres,feature.routeDistance);
                const double cosine = std::cos(relative.headingRadians);
                const double sine = std::sin(relative.headingRadians);
                for (double side : {-1.0,1.0})
                {
                    constexpr double warningPostLateral = 8.3;
                    const double lateral = side*warningPostLateral;
                    const float emitterX = static_cast<float>(relative.x+lateral*cosine);
                    const float emitterZ = static_cast<float>(relative.z-lateral*sine);
                    const float distance = std::hypot(emitterX-playerX,emitterZ-playerZ);
                    const float proximity = std::clamp(1.0f-distance/260.0f,0.0f,1.0f);
                    const float level = static_cast<float>(closure)*proximity*proximity*0.16f;
                    if (level <= bestLevel) continue;
                    bestLevel = level;
                    bestX = emitterX;
                    bestZ = emitterZ;
                    bestVariant = feature.variant;
                }
            }

        crossingWarningLevel_ = bestLevel;
        crossingWarning_->setVolumeProperty(
            bestLevel*settings_.trainVolume*(1.0f-0.18f*announcementDuck_));
        crossingWarning_->setPitchProperty((bestVariant & 1U) == 0U ? -0.025f : 0.025f);
        if (bestLevel > 0.0f)
        {
            XnaAudio::AudioListener listener;
            listener.setPositionProperty(Vector3(playerX,1.55f,playerZ));
            listener.setForwardProperty(Vector3(
                std::sin(playerYaw),0.0f,-std::cos(playerYaw)));
            XnaAudio::AudioEmitter emitter;
            emitter.setPositionProperty(Vector3(bestX,1.6f,bestZ));
            crossingWarning_->Apply3D(listener,emitter);
        }

        const bool audible = bestLevel > 0.001f;
        if (audible && !crossingWarningAudible_) ++crossingWarningEventCount_;
        crossingWarningAudible_ = audible;
    }

    static std::unique_ptr<XnaAudio::SoundEffect> load(const std::filesystem::path& path)
    {
        return std::make_unique<XnaAudio::SoundEffect>(path.string());
    }

    static std::unique_ptr<XnaAudio::SoundEffectInstance>
    makeLoop(XnaAudio::SoundEffect& effect, float volume)
    {
        auto instance = std::make_unique<XnaAudio::SoundEffectInstance>(effect.CreateInstance());
        instance->setIsLoopedProperty(true);
        instance->setVolumeProperty(volume);
        instance->Play();
        return instance;
    }

    static std::unique_ptr<XnaAudio::SoundEffectInstance>
    makeSpatialLoop(XnaAudio::SoundEffect& effect)
    {
        auto instance = std::make_unique<XnaAudio::SoundEffectInstance>(effect.CreateInstance());
        instance->setIsLoopedProperty(true);
        instance->setVolumeProperty(0.0f);
        XnaAudio::AudioListener listener;
        listener.setPositionProperty(Vector3(0.0f, 1.55f, -48.0f));
        listener.setForwardProperty(Vector3(0.0f, 0.0f, -1.0f));
        XnaAudio::AudioEmitter emitter;
        emitter.setPositionProperty(Vector3(0.0f, 1.0f, -42.0f));
        instance->Apply3D(listener, emitter);
        instance->Play();
        return instance;
    }

    static void playOneShot(XnaAudio::SoundEffect& effect,
                            std::unique_ptr<XnaAudio::SoundEffectInstance>& instance, float volume)
    {
        if (instance && instance->getStateProperty() != XnaAudio::SoundState::Stopped)
            instance->Stop(true);
        instance = std::make_unique<XnaAudio::SoundEffectInstance>(effect.CreateInstance());
        instance->setVolumeProperty(volume);
        instance->Play();
    }

    struct PendingAnnouncement
    {
        XnaAudio::SoundEffect* effect = nullptr;
        std::string caption;
        bool terminal = false;
    };

    [[nodiscard]] bool queueAnnouncement(XnaAudio::SoundEffect* effect, std::string caption,
                                         bool terminal = false)
    {
        if (effect == nullptr || announcementChimeEffect_ == nullptr
            || !announcementSequenceIdle()) return false;
        pendingAnnouncement_ = PendingAnnouncement{effect,std::move(caption),terminal};
        stop(announcementChime_);
        announcementChime_ = std::make_unique<XnaAudio::SoundEffectInstance>(
            announcementChimeEffect_->CreateInstance());
        announcementChime_->setVolumeProperty(0.58f * settings_.announcementVolume);
        announcementChime_->Play();
        announcementChimeRemaining_ = kAnnouncementChimeSeconds;
        ++announcementChimeEventCount_;
        return true;
    }

    void advancePendingAnnouncement(double realSeconds)
    {
        if (!pendingAnnouncement_.has_value()) return;
        announcementChimeRemaining_ = std::max(
            0.0,announcementChimeRemaining_-std::max(0.0,realSeconds));
        if (announcementChimeRemaining_ > 0.0) return;
        PendingAnnouncement pending = std::move(*pendingAnnouncement_);
        pendingAnnouncement_.reset();
        stop(announcementChime_);
        announcement_ = std::make_unique<XnaAudio::SoundEffectInstance>(
            pending.effect->CreateInstance());
        announcementCaption_ = std::move(pending.caption);
        lastAnnouncementCaption_ = announcementCaption_;
        announcement_->setVolumeProperty(settings_.announcementVolume);
        announcement_->Play();
        ++announcementEventCount_;
        if (pending.terminal) ++terminalAnnouncementEventCount_;
    }

    [[nodiscard]] bool announcementStopped() const
    {
        return !announcement_ || announcement_->getStateProperty() == XnaAudio::SoundState::Stopped;
    }

    [[nodiscard]] bool announcementSequenceIdle() const
    {
        return !pendingAnnouncement_.has_value() && announcementStopped();
    }

    [[nodiscard]] double nextMurmurRandom()
    {
        murmurRandomState_ ^= murmurRandomState_ >> 12;
        murmurRandomState_ ^= murmurRandomState_ << 25;
        murmurRandomState_ ^= murmurRandomState_ >> 27;
        const std::uint64_t value = murmurRandomState_ * 2685821657736338717ULL;
        return static_cast<double>(value >> 11) * (1.0 / 9007199254740992.0);
    }

    void positionMurmur(const Sim::Passenger& passenger, float playerX, float playerZ,
                        float playerYaw)
    {
        if (!murmur_) return;
        XnaAudio::AudioListener listener;
        listener.setPositionProperty(Vector3(playerX, 1.55f, playerZ));
        listener.setForwardProperty(Vector3(std::sin(playerYaw), 0.0f, -std::cos(playerYaw)));
        XnaAudio::AudioEmitter emitter;
        emitter.setPositionProperty(Vector3(static_cast<float>(passenger.x), 1.25f,
                                            static_cast<float>(passenger.z)));
        murmur_->Apply3D(listener, emitter);
    }

    void updateMurmur(double realSeconds, const Sim::PassengerSystem& passengers,
                      float playerX, float playerZ, float playerYaw)
    {
        if (murmur_)
        {
            murmur_->setVolumeProperty(
                settings_.passengerVolume*(1.0f-0.82f*announcementDuck_));
            murmurRemaining_ -= realSeconds;
            if (murmurRemaining_ <= 0.0
                || murmur_->getStateProperty() == XnaAudio::SoundState::Stopped)
            {
                stop(murmur_);
                murmurPassenger_.reset();
                murmurCooldown_ = 5.0 + nextMurmurRandom() * 9.0;
                return;
            }

            if (murmurPassenger_.has_value()
                && *murmurPassenger_ < passengers.passengers().size())
                positionMurmur(passengers.passengers()[*murmurPassenger_],
                               playerX, playerZ, playerYaw);
            return;
        }

        murmurCooldown_ -= realSeconds;
        if (murmurCooldown_ > 0.0) return;

        auto& nearby = nearbyPassengerScratch_;
        auto& talking = talkingPassengerScratch_;
        nearby.clear();
        talking.clear();
        for (const auto& passenger : passengers.passengers())
        {
            if (passenger.role != Sim::PassengerRole::Passenger
                || passenger.state == Sim::PassengerState::Offboard
                || passenger.state == Sim::PassengerState::Boarding
                || passenger.state == Sim::PassengerState::Alighting
                || passenger.state == Sim::PassengerState::PreparingToLeave
                || passenger.state == Sim::PassengerState::VisitingToilet
                || passenger.state == Sim::PassengerState::UsingToilet
                || passenger.state == Sim::PassengerState::LeavingToilet) continue;
            const double distance = std::hypot(static_cast<double>(playerX) - passenger.x,
                                               static_cast<double>(playerZ) - passenger.z);
            if (distance <= 18.0)
            {
                nearby.push_back(&passenger);
                if (passenger.state == Sim::PassengerState::Talking)
                    talking.push_back(&passenger);
            }
        }
        if (nearby.empty())
        {
            murmurCooldown_ = 2.0;
            return;
        }

        const auto& candidates = talking.empty() ? nearby : talking;
        const auto choice = static_cast<std::size_t>(
                                nextMurmurRandom() * static_cast<double>(candidates.size()))
                          % candidates.size();
        const Sim::Passenger& speaker = *candidates[choice];
        const std::size_t variant = (static_cast<std::size_t>(speaker.appearanceSeed)
            + static_cast<std::size_t>(nextMurmurRandom() * 17.0)) % murmurEffects_.size();
        murmur_ = std::make_unique<XnaAudio::SoundEffectInstance>(
            murmurEffects_[variant]->CreateInstance());
        murmur_->setIsLoopedProperty(false);
        murmur_->setVolumeProperty(
            settings_.passengerVolume*(1.0f-0.82f*announcementDuck_));
        const float voicePitch = -0.08f
            + static_cast<float>(speaker.appearanceSeed % 9U) * 0.02f;
        murmur_->setPitchProperty(voicePitch);
        positionMurmur(speaker, playerX, playerZ, playerYaw);
        murmur_->Play();
        ++passengerMurmurEventCount_;
        murmurPassenger_ = speaker.id;
        murmurRemaining_ = 1.8 + nextMurmurRandom() * 3.4;
    }

    static void stop(std::unique_ptr<XnaAudio::SoundEffectInstance>& instance) noexcept
    {
        if (!instance) return;
        try { instance->Stop(true); }
        catch (...)
        {
            // Audio teardown is best-effort and this noexcept path must still release ownership.
            instance.reset();
            return;
        }
        instance.reset();
    }

    void stopAll() noexcept
    {
        pendingAnnouncement_.reset();
        announcementChimeRemaining_ = 0.0;
        stop(announcement_);
        stop(announcementChime_);
        stop(doorInstance_);
        stop(doorWarningInstance_);
        stop(murmur_);
        stop(footstep_);
        stop(gangwayDoor_);
        stop(toiletDoor_);
        stop(passingTrain_);
        stop(crossingWarning_);
        stop(stationAmbience_);
        stop(interiorHvac_);
        stop(rain_);
        stop(gangwayRattle_);
        stop(railTexture_);
        stop(curveSqueal_);
        stop(braking_);
        stop(bridge_);
        stop(tunnel_);
        stop(wind_);
        stop(traction_);
        stop(rolling_);
    }

    std::unique_ptr<XnaAudio::SoundEffect> rollingEffect_;
    TrainAudioSettings settings_;
    std::unique_ptr<XnaAudio::SoundEffect> tractionEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> windEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> tunnelEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> bridgeEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> brakingEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> curveSquealEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> railTextureEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> gangwayRattleEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> gangwayDoorEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> toiletDoorEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> passingTrainEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> crossingWarningEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> rainEffect_;
    std::vector<std::unique_ptr<XnaAudio::SoundEffect>> carpetFootstepEffects_;
    std::vector<std::unique_ptr<XnaAudio::SoundEffect>> nonCarpetedFootstepEffects_;
    std::vector<std::unique_ptr<XnaAudio::SoundEffect>> murmurEffects_;
    std::vector<const Sim::Passenger*> nearbyPassengerScratch_;
    std::vector<const Sim::Passenger*> talkingPassengerScratch_;
    std::unique_ptr<XnaAudio::SoundEffect> stationAmbienceEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> interiorHvacEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> announcementChimeEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> doorWarningEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> doorOpenEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> doorCloseEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> welcomeWesthavenEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> welcomeAlderwickEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> reverseAlderwickEffect_;
    std::unique_ptr<XnaAudio::SoundEffect> reverseWesthavenEffect_;
    std::vector<std::unique_ptr<XnaAudio::SoundEffect>> stationEffects_;
    std::vector<std::unique_ptr<XnaAudio::SoundEffect>> stationAlternateEffects_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> rolling_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> traction_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> wind_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> tunnel_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> bridge_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> braking_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> curveSqueal_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> railTexture_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> gangwayRattle_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> gangwayDoor_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> toiletDoor_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> passingTrain_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> crossingWarning_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> footstep_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> murmur_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> stationAmbience_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> interiorHvac_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> rain_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> doorInstance_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> doorWarningInstance_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> announcementChime_;
    std::unique_ptr<XnaAudio::SoundEffectInstance> announcement_;
    std::string announcementCaption_;
    std::string lastAnnouncementCaption_;
    std::optional<PendingAnnouncement> pendingAnnouncement_;
    std::optional<std::size_t> announcedStation_;
    std::optional<std::size_t> murmurPassenger_;
    std::uint64_t previousCompletedLegs_ = 0;
    std::uint64_t murmurRandomState_ = 0x535045454348ULL;
    double elapsed_ = 0.0;
    double pendingDoorClose_ = 0.0;
    double murmurCooldown_ = 8.0;
    double murmurRemaining_ = 0.0;
    double footstepCooldown_ = 0.0;
    float footstepBaseVolume_ = 0.13f;
    double passingTrainCooldown_ = 0.0;
    double announcementChimeRemaining_ = 0.0;
    float tunnelMix_ = 0.0f;
    float bridgeMix_ = 0.0f;
    float stationMix_ = 0.0f;
    float gangwayZ_ = 0.0f;
    float gangwayDoorZ_ = 0.0f;
    float gangwayProximity_ = 0.0f;
    float speedRatio_ = 0.0f;
    float curveAmount_ = 0.0f;
    float tractionEffort_ = 0.0f;
    float brakingEffort_ = 0.0f;
    float crossingWarningLevel_ = 0.0f;
    float announcementDuck_ = 0.0f;
    float rainMix_ = 0.0f;
    Sim::WeatherMode weatherMode_ = Sim::WeatherMode::Clear;
    bool crossingWarningAudible_ = false;
    bool previousDoorsOpen_ = true;
    bool previousToiletDoorOpen_ = false;
    std::array<bool, Sim::TrainInterior::GangwayCount> previousGangwaySensors_{};
    std::size_t footstepSequence_ = 0;
    std::size_t passingTrainEventCount_ = 0;
    std::size_t announcementChimeEventCount_ = 0;
    std::size_t announcementEventCount_ = 0;
    std::size_t terminalAnnouncementEventCount_ = 0;
    std::size_t passengerMurmurEventCount_ = 0;
    std::size_t doorTransitionEventCount_ = 0;
    std::size_t toiletDoorEventCount_ = 0;
    std::size_t crossingWarningEventCount_ = 0;
    std::size_t carpetFootstepEventCount_ = 0;
    std::size_t nonCarpetedFootstepEventCount_ = 0;
    bool welcomePlayed_ = false;
    std::optional<int> pendingTerminalDirection_;
    bool available_ = false;
    bool paused_ = false;
};

TrainAudio::TrainAudio(const std::filesystem::path& assetDirectory, TrainAudioSettings settings,
                       bool passengerDoorsInitiallyOpen, bool toiletDoorInitiallyOpen)
    : impl_(std::make_unique<Impl>(assetDirectory, settings, passengerDoorsInitiallyOpen,
                                  toiletDoorInitiallyOpen)) {}

TrainAudio::~TrainAudio() = default;

void TrainAudio::update(double realSeconds, const Sim::Route& route,
                        const Sim::RouteSimulation& service,
                        const Sim::PassengerSystem& passengers, const World::StreamingWorld& world,
                        float playerX, float playerZ,
                        float playerYaw, bool playerWalking, bool toiletDoorOpen,
                        bool inTunnel, bool onBridge)
{
    impl_->update(realSeconds, route, service, passengers, world, playerX, playerZ, playerYaw,
                  playerWalking, toiletDoorOpen, inTunnel, onBridge);
}

bool TrainAudio::available() const noexcept { return impl_->available(); }

void TrainAudio::setPaused(bool paused) noexcept { impl_->setPaused(paused); }

bool TrainAudio::paused() const noexcept { return impl_->paused(); }

std::size_t TrainAudio::playingInstanceCount() const noexcept
{
    return impl_->playingInstanceCount();
}

std::size_t TrainAudio::pausedInstanceCount() const noexcept
{
    return impl_->pausedInstanceCount();
}

std::string_view TrainAudio::currentAnnouncementCaption() const noexcept
{
    return impl_->currentAnnouncementCaption();
}

std::string_view TrainAudio::lastAnnouncementCaption() const noexcept
{
    return impl_->lastAnnouncementCaption();
}

std::string_view TrainAudio::queuedAnnouncementCaption() const noexcept
{
    return impl_->queuedAnnouncementCaption();
}

std::size_t TrainAudio::announcementChimeEventCount() const noexcept
{
    return impl_->announcementChimeEventCount();
}

std::size_t TrainAudio::announcementEventCount() const noexcept
{
    return impl_->announcementEventCount();
}

std::size_t TrainAudio::terminalAnnouncementEventCount() const noexcept
{
    return impl_->terminalAnnouncementEventCount();
}

std::size_t TrainAudio::passengerMurmurEventCount() const noexcept
{
    return impl_->passengerMurmurEventCount();
}

std::size_t TrainAudio::passingTrainEventCount() const noexcept
{
    return impl_->passingTrainEventCount();
}

std::size_t TrainAudio::doorTransitionEventCount() const noexcept
{
    return impl_->doorTransitionEventCount();
}

std::size_t TrainAudio::toiletDoorEventCount() const noexcept
{
    return impl_->toiletDoorEventCount();
}

std::size_t TrainAudio::crossingWarningEventCount() const noexcept
{
    return impl_->crossingWarningEventCount();
}

std::size_t TrainAudio::carpetFootstepEventCount() const noexcept
{
    return impl_->carpetFootstepEventCount();
}

std::size_t TrainAudio::nonCarpetedFootstepEventCount() const noexcept
{
    return impl_->nonCarpetedFootstepEventCount();
}

bool TrainAudio::tunnelAcousticsActive() const noexcept
{
    return impl_->tunnelAcousticsActive();
}

bool TrainAudio::bridgeAcousticsActive() const noexcept
{
    return impl_->bridgeAcousticsActive();
}

bool TrainAudio::interiorAmbienceActive() const noexcept
{
    return impl_->interiorAmbienceActive();
}

float TrainAudio::tractionEffort() const noexcept { return impl_->tractionEffort(); }

float TrainAudio::brakingEffort() const noexcept { return impl_->brakingEffort(); }

void TrainAudio::setSettings(TrainAudioSettings settings) { impl_->setSettings(settings); }
void TrainAudio::setWeather(Sim::WeatherMode mode) noexcept { impl_->setWeather(mode); }
float TrainAudio::rainLevel() const noexcept { return impl_->rainLevel(); }

} // namespace TrainSim::Audio

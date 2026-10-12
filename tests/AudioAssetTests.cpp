#include "TestHarness.hpp"

#include "trainsim/audio/TrainAudio.hpp"
#include "trainsim/sim/Route.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {

bool isWaveFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::array<char, 12> header{};
    input.read(header.data(), static_cast<std::streamsize>(header.size()));
    return input.gcount() == static_cast<std::streamsize>(header.size())
        && std::string(header.data(), 4) == "RIFF"
        && std::string(header.data() + 8, 4) == "WAVE"
        && std::filesystem::file_size(path) > 44;
}

std::uint64_t fileFingerprint(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ULL;
    char byte = 0;
    while (input.get(byte))
    {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

TEST_CASE("runtime audio manifest contains every generated railway and announcement clip")
{
    const std::filesystem::path audio =
        std::filesystem::path(TRAINSIM_TEST_ASSET_DIR) / "audio";
    const std::vector<std::string> ambience = {
        "rolling.wav", "traction.wav", "wind.wav", "tunnel_rumble.wav",
        "bridge_rumble.wav", "passing_train.wav", "crossing_warning.wav",
        "braking.wav", "curve_squeal.wav",
        "rail_texture.wav",
        "gangway_rattle.wav", "gangway_door.wav", "toilet_door.wav",
        "station_ambience.wav", "interior_hvac.wav", "announcement_chime.wav", "rain.wav",
        "passenger_murmur.wav",
        "passenger_murmur_01.wav", "passenger_murmur_02.wav", "passenger_murmur_03.wav",
        "footstep_carpet_01.wav", "footstep_carpet_02.wav",
        "footstep_vestibule_01.wav", "footstep_vestibule_02.wav",
        "door_warning.wav", "door_open.wav", "door_close.wav",
        "announcement_welcome.wav", "announcement_welcome_alderwick.wav",
        "announcement_reverse_to_alderwick.wav", "announcement_reverse_to_westhaven.wav",
    };
    for (const std::string& name : ambience) REQUIRE(isWaveFile(audio / name));

    const std::array<std::string, 4> murmurVariants = {
        "passenger_murmur.wav", "passenger_murmur_01.wav",
        "passenger_murmur_02.wav", "passenger_murmur_03.wav"};
    std::set<std::uint64_t> murmurFingerprints;
    for (const std::string& name : murmurVariants)
        murmurFingerprints.insert(fileFingerprint(audio / name));
    REQUIRE(murmurFingerprints.size() == murmurVariants.size());

    const std::array<std::string, 4> footstepVariants = {
        "footstep_carpet_01.wav", "footstep_carpet_02.wav",
        "footstep_vestibule_01.wav", "footstep_vestibule_02.wav"};
    std::set<std::uint64_t> footstepFingerprints;
    for (const std::string& name : footstepVariants)
        footstepFingerprints.insert(fileFingerprint(audio / name));
    REQUIRE(footstepFingerprints.size() == footstepVariants.size());

    for (int station = 0; station < 12; ++station)
    {
        const std::string suffix = station < 10 ? "0" + std::to_string(station)
                                                : std::to_string(station);
        REQUIRE(isWaveFile(audio / ("announcement_station_" + suffix + ".wav")));
        REQUIRE(isWaveFile(audio / ("announcement_station_" + suffix + "_alternate.wav")));
    }
}

TEST_CASE("announcement captions follow every route station and both service directions")
{
    const TrainSim::Sim::Route route = TrainSim::Sim::Route::NorthernValleyExpress();
    const std::string outward = TrainSim::Audio::welcomeAnnouncementCaption(route,1);
    const std::string returning = TrainSim::Audio::welcomeAnnouncementCaption(route,-1);
    REQUIRE(outward.find(route.stations().back().name) != std::string::npos);
    REQUIRE(returning.find(route.stations().front().name) != std::string::npos);

    for (std::size_t index = 0; index < route.stations().size(); ++index)
    {
        const std::string normal =
            TrainSim::Audio::stationAnnouncementCaption(route,index,false);
        const std::string alternate =
            TrainSim::Audio::stationAnnouncementCaption(route,index,true);
        REQUIRE(normal.find("THE NEXT STATION IS "+route.stations()[index].name)
                != std::string::npos);
        REQUIRE(normal.find("MIND THE GAP BETWEEN THE TRAIN AND THE PLATFORM")
                != std::string::npos);
        REQUIRE(alternate.find("WE ARE NOW APPROACHING "+route.stations()[index].name)
                != std::string::npos);
        REQUIRE(alternate.find("ALL YOUR BELONGINGS") != std::string::npos);
    }

    const std::string atWesthaven = TrainSim::Audio::terminalAnnouncementCaption(
        route,route.stations().size()-1U,-1);
    REQUIRE(atWesthaven.find(route.stations().back().name) != std::string::npos);
    REQUIRE(atWesthaven.find("CONTINUE TO "+route.stations().front().name)
            != std::string::npos);
    const std::string atAlderwick =
        TrainSim::Audio::terminalAnnouncementCaption(route,0,1);
    REQUIRE(atAlderwick.find(route.stations().front().name) != std::string::npos);
    REQUIRE(atAlderwick.find("CONTINUE TO "+route.stations().back().name)
            != std::string::npos);
}

TEST_CASE("announcement ducking rises quickly and restores the ambience smoothly")
{
    using TrainSim::Audio::advanceAnnouncementDuck;
    REQUIRE(advanceAnnouncementDuck(0.0f,true,0.10) > 0.20f);
    REQUIRE(advanceAnnouncementDuck(0.9f,true,1.0) == 1.0f);
    REQUIRE(advanceAnnouncementDuck(1.0f,false,0.20) == 0.75f);
    REQUIRE(advanceAnnouncementDuck(0.1f,false,1.0) == 0.0f);
    REQUIRE(advanceAnnouncementDuck(0.4f,true,-1.0) == 0.4f);
    REQUIRE(advanceAnnouncementDuck(
        std::numeric_limits<float>::quiet_NaN(),true,0.0) == 0.0f);
}

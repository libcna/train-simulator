#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace TrainSim::Sim {

// Time of day and weather (ENV-138/ENV-139). Both follow simulated service time, never host
// time, so accelerated QA, a frozen train and the station clocks all stay coherent.

// The Northern Valley service day begins at 09:17 unless the player chooses another start.
inline constexpr double kDefaultServiceStartHours = 557.0/60.0;

// Clock time in hours [0, 24) after the given simulated service seconds.
[[nodiscard]] double serviceClockHours(double startHours, double serviceElapsedSeconds) noexcept;

// "HH:MM" (or "H:MM") to minutes after midnight; nullopt if malformed or out of range.
[[nodiscard]] std::optional<int> parseClockTime(std::string_view text) noexcept;

enum class Weather : std::uint8_t { Clear, Cloudy, Overcast, Rain, Fog, Count };
// Fixed weather, or a deterministic itinerary that changes with service time.
enum class WeatherMode : std::uint8_t { Clear, Cloudy, Overcast, Rain, Fog, Changing, Count };

[[nodiscard]] std::string_view weatherModeName(WeatherMode mode) noexcept;
[[nodiscard]] std::optional<WeatherMode> parseWeatherMode(std::string_view text) noexcept;

// Continuous conditions, so the renderer and audio can cross-fade between weather types.
struct WeatherConditions
{
    float cloudCover = 0.0f; // Fraction of the sky with cloud: 0 clear .. 1 closed deck.
    float overcast = 0.0f;   // Grey diffuse light replacing direct sun: 0 .. 1.
    float rain = 0.0f;       // Precipitation intensity: 0 .. 1.
    float fog = 0.0f;        // Ground fog density: 0 .. 1.
    Weather dominant = Weather::Clear;
    bool operator==(const WeatherConditions&) const = default;
};

[[nodiscard]] WeatherConditions weatherConditions(Weather weather) noexcept;
[[nodiscard]] WeatherConditions weatherConditions(WeatherMode mode,
                                                  double serviceElapsedSeconds) noexcept;

// The changing itinerary holds each weather for this long, blending into the next one over the
// final kWeatherTransitionSeconds of each period.
inline constexpr double kWeatherPeriodSeconds = 15.0*60.0;
inline constexpr double kWeatherTransitionSeconds = 4.0*60.0;

} // namespace TrainSim::Sim

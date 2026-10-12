#include "trainsim/sim/Environment.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace TrainSim::Sim {

namespace {
// Three hours of plausible, gradual weather: no jump from clear sky straight into rain or fog.
constexpr std::array<Weather,12> kItinerary{
    Weather::Clear, Weather::Cloudy, Weather::Overcast, Weather::Rain, Weather::Rain,
    Weather::Overcast, Weather::Cloudy, Weather::Clear, Weather::Clear, Weather::Cloudy,
    Weather::Fog, Weather::Cloudy};

WeatherConditions blend(const WeatherConditions& a, const WeatherConditions& b, float t) noexcept
{
    const auto mix = [t](float x, float y) { return x + (y-x)*t; };
    return {mix(a.cloudCover,b.cloudCover), mix(a.overcast,b.overcast), mix(a.rain,b.rain),
            mix(a.fog,b.fog), t < 0.5f ? a.dominant : b.dominant};
}
} // namespace

double serviceClockHours(double startHours, double serviceElapsedSeconds) noexcept
{
    const double start = std::isfinite(startHours) ? startHours : kDefaultServiceStartHours;
    const double elapsed = std::isfinite(serviceElapsedSeconds)
        ? std::max(0.0,serviceElapsedSeconds) : 0.0;
    const double hours = std::fmod(start + elapsed/3'600.0, 24.0);
    return hours < 0.0 ? hours + 24.0 : hours;
}

std::optional<int> parseClockTime(std::string_view text) noexcept
{
    const auto colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon > 2 || text.size() != colon + 3)
        return std::nullopt;
    int hours = 0, minutes = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (i == colon) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return std::nullopt;
        int& field = i < colon ? hours : minutes;
        field = field*10 + (text[i]-'0');
    }
    if (hours > 23 || minutes > 59) return std::nullopt;
    return hours*60 + minutes;
}

std::string_view weatherModeName(WeatherMode mode) noexcept
{
    switch (mode)
    {
        case WeatherMode::Clear: return "clear";
        case WeatherMode::Cloudy: return "cloudy";
        case WeatherMode::Overcast: return "overcast";
        case WeatherMode::Rain: return "rain";
        case WeatherMode::Fog: return "fog";
        case WeatherMode::Changing: return "changing";
        default: return "clear";
    }
}

std::optional<WeatherMode> parseWeatherMode(std::string_view text) noexcept
{
    for (std::size_t mode = 0; mode < static_cast<std::size_t>(WeatherMode::Count); ++mode)
        if (text == weatherModeName(static_cast<WeatherMode>(mode)))
            return static_cast<WeatherMode>(mode);
    return std::nullopt;
}

WeatherConditions weatherConditions(Weather weather) noexcept
{
    switch (weather)
    {
        case Weather::Cloudy: return {0.55f,0.30f,0.0f,0.0f,Weather::Cloudy};
        case Weather::Overcast: return {1.0f,1.0f,0.0f,0.12f,Weather::Overcast};
        case Weather::Rain: return {1.0f,1.0f,1.0f,0.35f,Weather::Rain};
        case Weather::Fog: return {0.75f,0.85f,0.0f,1.0f,Weather::Fog};
        default: return {};
    }
}

WeatherConditions weatherConditions(WeatherMode mode, double serviceElapsedSeconds) noexcept
{
    if (mode != WeatherMode::Changing)
        return weatherConditions(mode < WeatherMode::Changing
            ? static_cast<Weather>(mode) : Weather::Clear);
    const double elapsed = std::isfinite(serviceElapsedSeconds)
        ? std::max(0.0,serviceElapsedSeconds) : 0.0;
    const auto period = static_cast<std::uint64_t>(elapsed/kWeatherPeriodSeconds);
    const double into = elapsed - static_cast<double>(period)*kWeatherPeriodSeconds;
    const Weather current = kItinerary[period % kItinerary.size()];
    const Weather next = kItinerary[(period+1) % kItinerary.size()];
    const double transitionStart = kWeatherPeriodSeconds - kWeatherTransitionSeconds;
    const float raw = static_cast<float>(std::clamp(
        (into-transitionStart)/kWeatherTransitionSeconds,0.0,1.0));
    const float eased = raw*raw*(3.0f-2.0f*raw);
    return blend(weatherConditions(current),weatherConditions(next),eased);
}

} // namespace TrainSim::Sim

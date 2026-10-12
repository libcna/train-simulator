#include "TestHarness.hpp"
#include "trainsim/sim/Environment.hpp"

#include <cmath>
#include <limits>

using namespace TrainSim::Sim;

TEST_CASE("service clock starts at the configured time and wraps at midnight")
{
    REQUIRE(serviceClockHours(kDefaultServiceStartHours,0.0) == kDefaultServiceStartHours);
    REQUIRE(std::abs(serviceClockHours(9.0,5'400.0)-10.5) < 1e-12);
    REQUIRE(std::abs(serviceClockHours(23.5,3'600.0)-0.5) < 1e-12);
    REQUIRE(serviceClockHours(8.0,-50.0) == 8.0);
    REQUIRE(serviceClockHours(std::numeric_limits<double>::quiet_NaN(),0.0)
            == kDefaultServiceStartHours);
    REQUIRE(serviceClockHours(8.0,std::numeric_limits<double>::infinity()) == 8.0);

    REQUIRE(parseClockTime("09:17") == 557);
    REQUIRE(parseClockTime("7:05") == 425);
    REQUIRE(parseClockTime("23:59") == 1'439);
    REQUIRE(!parseClockTime("24:00"));
    REQUIRE(!parseClockTime("12:60"));
    REQUIRE(!parseClockTime("1200"));
    REQUIRE(!parseClockTime("12:5"));
    REQUIRE(!parseClockTime("a2:00"));
    REQUIRE(!parseClockTime(""));
}

TEST_CASE("weather modes round-trip their names and fixed modes stay constant")
{
    for (std::size_t index = 0; index < static_cast<std::size_t>(WeatherMode::Count); ++index)
    {
        const auto mode = static_cast<WeatherMode>(index);
        REQUIRE(parseWeatherMode(weatherModeName(mode)) == mode);
    }
    REQUIRE(!parseWeatherMode("snow"));
    REQUIRE(weatherConditions(WeatherMode::Clear,0.0) == WeatherConditions{});
    for (WeatherMode mode : {WeatherMode::Cloudy,WeatherMode::Overcast,WeatherMode::Rain,
                             WeatherMode::Fog})
        REQUIRE(weatherConditions(mode,0.0) == weatherConditions(mode,12'345.0));
    REQUIRE(weatherConditions(WeatherMode::Rain,0.0).rain == 1.0f);
    REQUIRE(weatherConditions(WeatherMode::Fog,0.0).fog == 1.0f);
    REQUIRE(weatherConditions(WeatherMode::Overcast,0.0).overcast == 1.0f);
    REQUIRE(weatherConditions(WeatherMode::Cloudy,0.0).overcast < 0.5f);
}

TEST_CASE("changing weather follows a gradual deterministic itinerary")
{
    // The itinerary starts clear, holds each weather and blends only at the end of a period.
    REQUIRE(weatherConditions(WeatherMode::Changing,0.0).dominant == Weather::Clear);
    REQUIRE(weatherConditions(WeatherMode::Changing,kWeatherPeriodSeconds*0.5)
            == weatherConditions(Weather::Clear));
    const double transition = kWeatherPeriodSeconds-kWeatherTransitionSeconds*0.5;
    const WeatherConditions blending = weatherConditions(WeatherMode::Changing,transition);
    REQUIRE(blending.cloudCover > 0.0f);
    REQUIRE(blending.cloudCover < weatherConditions(Weather::Cloudy).cloudCover);
    bool sawRain = false, sawFog = false;
    WeatherConditions previous = weatherConditions(WeatherMode::Changing,0.0);
    for (double seconds = 0.0; seconds < kWeatherPeriodSeconds*13.0; seconds += 5.0)
    {
        const WeatherConditions now = weatherConditions(WeatherMode::Changing,seconds);
        sawRain = sawRain || now.rain > 0.99f;
        sawFog = sawFog || now.fog > 0.99f;
        for (float value : {now.cloudCover,now.overcast,now.rain,now.fog})
            REQUIRE(value >= 0.0f && value <= 1.0f);
        // Five simulated seconds never change any parameter by more than a few percent.
        REQUIRE(std::abs(now.rain-previous.rain) < 0.05f);
        REQUIRE(std::abs(now.fog-previous.fog) < 0.05f);
        REQUIRE(std::abs(now.overcast-previous.overcast) < 0.05f);
        previous = now;
    }
    REQUIRE(sawRain);
    REQUIRE(sawFog);
    // The three-hour itinerary repeats exactly.
    REQUIRE(weatherConditions(WeatherMode::Changing,1'000.0)
            == weatherConditions(WeatherMode::Changing,1'000.0+kWeatherPeriodSeconds*12.0));
}

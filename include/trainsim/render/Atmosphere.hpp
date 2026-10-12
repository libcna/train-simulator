#pragma once

#include "trainsim/render/Daylight.hpp"
#include "trainsim/sim/Environment.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <cmath>

namespace TrainSim::Render {

// Time of day and weather lighting (ENV-138/ENV-139). The service clock moves a fictional summer
// sun; weather replaces direct light with grey diffuse light, lowers the fog limit and adds rain.
// At the default 09:17 start in clear weather every value equals the earlier fixed daylight, so
// established captures and tests keep their exact appearance.

// Sun elevation and azimuth relative to the fixed reference daylight, in radians.
struct SunPosition
{
    double elevation = 0.0;
    double azimuthOffset = 0.0;
    bool operator==(const SunPosition&) const = default;
};

namespace atmosphere_detail {
inline constexpr double kPi = 3.14159265358979323846;
// The earlier fixed sun ray (0.42, -0.84, 0.34) has this elevation.
inline const double kReferenceElevation = std::asin(0.84/std::sqrt(0.42*0.42+0.84*0.84+0.34*0.34));
inline constexpr double kSunrise = 4.5, kSunset = 21.5;
inline constexpr double kNightDepth = 18.0*kPi/180.0;
inline double dayArc(double hours) { return std::sin(kPi*(hours-kSunrise)/(kSunset-kSunrise)); }

inline float smoothstep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x-edge0)/(edge1-edge0),0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
// Exact endpoints, so a fully weighted reference value is reproduced bit for bit.
inline Microsoft::Xna::Framework::Vector3 mix(const Microsoft::Xna::Framework::Vector3& a,
                                              const Microsoft::Xna::Framework::Vector3& b,float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a+(b-a)*t;
}
inline float mix(float a,float b,float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a+(b-a)*t;
}
// Sky colours stay in 0..255 units so the reference gradient reproduces its integer formula.
inline Microsoft::Xna::Framework::Vector3 bytes(int r,int g,int b)
{
    return {static_cast<float>(r),static_cast<float>(g),static_cast<float>(b)};
}
} // namespace atmosphere_detail

// Summer daylight from 04:30 to 21:30, peaking at about 74 degrees; the sun moves 15 degrees of
// azimuth an hour. Below the horizon it reaches 18 degrees of depression at about 01:00.
[[nodiscard]] inline SunPosition sunPosition(double clockHours)
{
    using namespace atmosphere_detail;
    const double reference = Sim::kDefaultServiceStartHours;
    double hours = std::fmod(std::isfinite(clockHours) ? clockHours : reference,24.0);
    if (hours < 0.0) hours += 24.0;
    SunPosition sun;
    if (hours >= kSunrise && hours <= kSunset)
        sun.elevation = kReferenceElevation*(dayArc(hours)/dayArc(reference));
    else
    {
        const double night = hours > kSunset ? hours-kSunset : hours+24.0-kSunset;
        sun.elevation = -kNightDepth*std::sin(kPi*night/(24.0-(kSunset-kSunrise)));
    }
    sun.azimuthOffset = (hours-reference)*15.0*kPi/180.0;
    return sun;
}

// Direction the sun's rays travel in the local route frame at a heading; equals
// daylightDirection(heading) for the reference sun.
[[nodiscard]] inline Microsoft::Xna::Framework::Vector3 sunRayDirection(double heading,
                                                                        const SunPosition& sun)
{
    using namespace atmosphere_detail;
    const double vertical = std::sin(sun.elevation)/std::sin(kReferenceElevation);
    const double horizontal = std::cos(sun.elevation)/std::cos(kReferenceElevation);
    const double ca = std::cos(sun.azimuthOffset), sa = std::sin(sun.azimuthOffset);
    const double bx = (0.42*ca-0.34*sa)*horizontal, bz = (0.42*sa+0.34*ca)*horizontal;
    const double c = std::cos(heading), s = std::sin(heading);
    return Microsoft::Xna::Framework::Vector3::Normalize({
        static_cast<float>(bx*c-bz*s),static_cast<float>(-0.84*vertical),
        static_cast<float>(bx*s+bz*c)});
}

// The moon stands opposite the sun at a fixed 38 degrees while it lights the night.
[[nodiscard]] inline SunPosition moonPosition(const SunPosition& sun)
{
    return {38.0*atmosphere_detail::kPi/180.0,sun.azimuthOffset+atmosphere_detail::kPi};
}

// Projected ground shadows are baked into streamed geometry; a low sun would stretch them far
// beyond their bounded footprints, so geometry uses at least 25 degrees and fades them instead.
[[nodiscard]] inline SunPosition shadowCastingPosition(const SunPosition& sun)
{
    if (sun.elevation <= 0.0) return moonPosition(sun);
    return {std::max(sun.elevation,25.0*atmosphere_detail::kPi/180.0),sun.azimuthOffset};
}

// Colours in 0..255 units.
struct SkyPalette
{
    Microsoft::Xna::Framework::Vector3 horizon, zenith;
    // Warm glow around the low sun, added toward the sun's azimuth near the horizon.
    Microsoft::Xna::Framework::Vector3 glow{0,0,0};
    float stars = 0.0f;
    bool operator==(const SkyPalette&) const = default;
};

struct Atmosphere
{
    SunPosition sun;
    // Key light: the sun by day, the moon at night (never from below the horizon).
    SunPosition keyLight;
    ExteriorLighting light;
    SkyPalette sky;
    Microsoft::Xna::Framework::Vector3 fogColour;
    float fogStart = 360.0f, fogEnd = 1'050.0f;
    float daylight = 1.0f;       // 1 full day .. 0 night: scales artificial-light contrast.
    float night = 0.0f;          // 0 day .. 1 night: lamps, lit windows and stars.
    float direct = 1.0f;         // Direct-sun share after cloud: sunlight patches, sparkle.
    float shadowStrength = 1.0f; // Alpha of the baked ground and contact shadows.
    float sunDisc = 1.0f;        // Visibility of the sun disc.
    float moonDisc = 0.0f;
    Microsoft::Xna::Framework::Vector3 sunTint{1,1,1};
    Microsoft::Xna::Framework::Vector3 cloudTint{1,1,1};
    float cloudCover = 0.0f;
    float rain = 0.0f;
    float overcast = 0.0f;
};

[[nodiscard]] inline Atmosphere atmosphere(double clockHours, const Sim::WeatherConditions& weather)
{
    using namespace atmosphere_detail;
    using Microsoft::Xna::Framework::Vector3;
    Atmosphere result;
    result.sun = sunPosition(clockHours);
    const float elevation = static_cast<float>(result.sun.elevation);
    const float overcast = std::clamp(weather.overcast,0.0f,1.0f);
    const float cover = std::clamp(weather.cloudCover,0.0f,1.0f);
    const float rain = std::clamp(weather.rain,0.0f,1.0f);
    const float fog = std::clamp(weather.fog,0.0f,1.0f);
    result.overcast = overcast;
    result.cloudCover = cover;
    result.rain = rain;

    // Elevation weights: full day above 20 degrees, golden light below, civil twilight to -6.
    const float day = smoothstep(-0.105f,0.35f,elevation);
    const float sunUp = smoothstep(-0.02f,0.35f,elevation);
    const float golden = 1.0f-smoothstep(0.04f,0.40f,elevation);
    const float twilight = (1.0f-smoothstep(0.0f,0.26f,std::abs(elevation+0.03f)))
                         * (1.0f-overcast*0.85f);
    result.daylight = day;
    result.night = 1.0f-smoothstep(-0.105f,0.06f,elevation);
    result.keyLight = elevation > 0.0f ? result.sun : moonPosition(result.sun);

    const ExteriorLighting clear = exteriorLighting(0.0f);
    const Vector3 nightAmbient{0.075f,0.090f,0.135f};
    const Vector3 moon{0.095f,0.105f,0.140f};
    // Direct sun weakens under cloud; a closed deck leaves only grey diffuse light.
    result.direct = (overcast <= 0.0f && cover <= 0.0f) ? 1.0f
        : std::clamp((1.0f-0.88f*overcast)*(1.0f-0.30f*cover*(1.0f-overcast)),0.0f,1.0f);
    const Vector3 warm = mix(Vector3{1,1,1},Vector3{1.0f,0.80f,0.55f},golden);
    Vector3 ambient = mix(nightAmbient,clear.ambient,day);
    const Vector3 overcastAmbient = mix(nightAmbient*0.85f,Vector3{0.50f,0.52f,0.55f},day);
    ambient = mix(ambient,overcastAmbient,overcast);
    if (rain > 0.0f) ambient = ambient*(1.0f-0.16f*rain);
    Vector3 diffuse = clear.diffuse*warm*sunUp;
    if (result.direct < 1.0f) diffuse = diffuse*result.direct;
    if (elevation <= 0.0f)
        diffuse = moon*result.night*(1.0f-0.80f*overcast);
    const float reflected = elevation <= 0.0f ? 0.0f
        : result.direct >= 1.0f ? sunUp : sunUp*(0.35f+0.65f*result.direct);
    result.light = {ambient,diffuse,
                    reflected >= 1.0f ? clear.bounce : clear.bounce*warm*reflected,
                    reflected >= 1.0f ? clear.uplight : clear.uplight*warm*reflected};

    // Sky: reference day palette, warm twilight, deep night, then grey under cloud.
    const Vector3 dayHorizon = bytes(169,201,222), dayZenith = bytes(69,136,198);
    const Vector3 duskHorizon = bytes(232,158,112), duskZenith = bytes(64,96,152);
    const Vector3 nightHorizon = bytes(22,29,44), nightZenith = bytes(5,8,19);
    Vector3 horizon = mix(nightHorizon,mix(duskHorizon,dayHorizon,1.0f-golden),day);
    Vector3 zenith = mix(nightZenith,mix(duskZenith,dayZenith,1.0f-golden),day);
    const float greyLevel = mix(0.10f,1.0f,day);
    horizon = mix(horizon,bytes(176,181,186)*greyLevel,overcast);
    zenith = mix(zenith,bytes(140,148,156)*greyLevel,overcast);
    horizon = mix(horizon,bytes(186,189,190)*greyLevel,fog);
    zenith = mix(zenith,bytes(170,174,177)*greyLevel,fog*0.8f);
    result.sky.horizon = horizon;
    result.sky.zenith = zenith;
    result.sky.glow = twilight > 0.0f ? Vector3{102.0f,41.0f,5.0f}*twilight : Vector3{0,0,0};
    result.sky.stars = result.night*(1.0f-overcast)*(1.0f-fog);

    // The reference fog is the integer horizon colour divided by 255, like rgb(Color).
    result.fogColour = Vector3{static_cast<float>(static_cast<int>(horizon.X))/255.0f,
                               static_cast<float>(static_cast<int>(horizon.Y))/255.0f,
                               static_cast<float>(static_cast<int>(horizon.Z))/255.0f};
    const float rainHaze = 1.0f-0.30f*rain;
    result.fogStart = (fog <= 0.0f && rain <= 0.0f) ? 360.0f : mix(360.0f,22.0f,fog)*rainHaze;
    result.fogEnd = (fog <= 0.0f && rain <= 0.0f) ? 1'050.0f : mix(1'050.0f,250.0f,fog)*rainHaze;

    // Shadows fade with the low sun and under cloud but keep a little contact darkening.
    const float castShadow = sunUp*result.direct;
    result.shadowStrength = castShadow >= 1.0f ? 1.0f : 0.30f+0.70f*castShadow;
    result.sunDisc = elevation > -0.02f
        ? smoothstep(-0.02f,0.03f,elevation)*(1.0f-overcast)*(1.0f-fog) : 0.0f;
    result.sunTint = mix(Vector3{1,1,1},Vector3{1.0f,0.62f,0.34f},golden);
    result.moonDisc = result.night*(1.0f-overcast)*(1.0f-fog);
    const Vector3 cloudDay = mix(Vector3{1,1,1},Vector3{1.0f,0.80f,0.66f},golden);
    Vector3 cloudTint = mix(Vector3{0.08f,0.09f,0.12f},cloudDay,day);
    cloudTint = mix(cloudTint,Vector3{0.66f,0.68f,0.71f}*greyLevel,overcast);
    if (rain > 0.0f) cloudTint = cloudTint*(1.0f-0.22f*rain);
    result.cloudTint = cloudTint;
    return result;
}

// Platforms are floodlit after dark: warm lamp fill replaces most of the moonlight on platform
// surfaces and the people waiting there. By day this is the open-air light unchanged.
[[nodiscard]] inline ExteriorLighting stationLighting(const Atmosphere& air)
{
    using atmosphere_detail::mix;
    const float lamps = air.night*0.9f;
    if (lamps <= 0.0f) return air.light;
    const ExteriorLighting lamp{{0.38f,0.35f,0.30f},{0.20f,0.18f,0.14f},
                                {0.0f,0.0f,0.0f},{0.06f,0.055f,0.045f}};
    return {mix(air.light.ambient,lamp.ambient,lamps),mix(air.light.diffuse,lamp.diffuse,lamps),
            mix(air.light.bounce,lamp.bounce,lamps),mix(air.light.uplight,lamp.uplight,lamps)};
}

// Sky colour toward a view direction: the reference vertical gradient plus twilight glow
// toward the sun's azimuth near the horizon. directionY is the normalised height in [-1, 1].
[[nodiscard]] inline Microsoft::Xna::Framework::Color skyColour(
    const SkyPalette& palette, float directionY, float sunwardness = 0.0f)
{
    using Microsoft::Xna::Framework::Color;
    const float t = std::pow(std::clamp(directionY,0.0f,1.0f),0.55f);
    const auto channel = [t](float horizon, float zenith) {
        return horizon + (zenith-horizon)*t;
    };
    float r = channel(palette.horizon.X,palette.zenith.X);
    float g = channel(palette.horizon.Y,palette.zenith.Y);
    float b = channel(palette.horizon.Z,palette.zenith.Z);
    if (palette.glow.X > 0.0f || palette.glow.Y > 0.0f || palette.glow.Z > 0.0f)
    {
        const float lowness = 1.0f-std::clamp(std::abs(directionY)/0.45f,0.0f,1.0f);
        const float towardSun = std::pow(std::clamp(0.5f+0.5f*sunwardness,0.0f,1.0f),3.0f);
        const float weight = lowness*lowness*(0.35f+0.65f*towardSun);
        r += palette.glow.X*weight;
        g += palette.glow.Y*weight;
        b += palette.glow.Z*weight;
    }
    const auto clampByte = [](float v) { return std::clamp(static_cast<int>(v),0,255); };
    return Color(clampByte(r),clampByte(g),clampByte(b),255);
}

// Whether the sky mesh should be rebuilt: palettes drift continuously with the clock, but a
// change below one colour step is invisible.
[[nodiscard]] inline bool skyPaletteChanged(const SkyPalette& a, const SkyPalette& b)
{
    const auto far = [](const Microsoft::Xna::Framework::Vector3& x,
                        const Microsoft::Xna::Framework::Vector3& y) {
        return std::abs(x.X-y.X) > 0.75f || std::abs(x.Y-y.Y) > 0.75f
            || std::abs(x.Z-y.Z) > 0.75f;
    };
    return far(a.horizon,b.horizon) || far(a.zenith,b.zenith) || far(a.glow,b.glow)
        || std::abs(a.stars-b.stars) > 0.01f;
}

} // namespace TrainSim::Render

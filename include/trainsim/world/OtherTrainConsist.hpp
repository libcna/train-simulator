#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace TrainSim::World {

// Oncoming and parked traffic (ENV-137). A moving OtherTrain feature keeps its motion phase in
// the low two variant bits; the consist type lives above them so phase-only fixtures (variant
// 0..2) remain the original three-car regional unit.
enum class OtherTrainType : std::uint8_t { RegionalEmu, Intercity, Freight, DieselUnit, Count };

enum class OtherVehicleKind : std::uint8_t {
    EmuLeadingCar, EmuMotorCar, EmuTrailingCar,
    IntercityLocomotive, IntercityCoach, IntercityTailCoach,
    FreightLocomotive, ContainerFlatPair, ContainerFlatLong, TankWagon, CoveredHopper, OpenWagon,
    DieselLeadingCar, DieselTrailingCar,
    Count
};

struct OtherTrainVehicle
{
    OtherVehicleKind kind = OtherVehicleKind::EmuMotorCar;
    double lengthMetres = 19.0; // Over couplers: the spacing this vehicle occupies in the consist.
    int cabEnds = 0;
};

struct OtherTrainConsist
{
    std::span<const OtherTrainVehicle> vehicles;
    double speedMetresPerSecond = 31.0;
    [[nodiscard]] constexpr double lengthMetres() const noexcept
    {
        double total = 0.0;
        for (const OtherTrainVehicle& vehicle : vehicles) total += vehicle.lengthMetres;
        return total;
    }
    // Signed offset of a vehicle centre from the consist centre, toward the leading end. The
    // leading vehicle (index zero) is the one nearest the direction of travel.
    [[nodiscard]] constexpr double leadOffsetMetres(std::size_t index) const noexcept
    {
        double front = 0.0;
        for (std::size_t vehicle = 0; vehicle < index && vehicle < vehicles.size(); ++vehicle)
            front += vehicles[vehicle].lengthMetres;
        const double own = index < vehicles.size() ? vehicles[index].lengthMetres : 0.0;
        return lengthMetres()*0.5 - front - own*0.5;
    }
};

namespace detail {
using enum OtherVehicleKind;
inline constexpr std::array<OtherTrainVehicle,3> kRegionalEmu{{
    {EmuLeadingCar,19.0,1},{EmuMotorCar,19.0,0},{EmuTrailingCar,19.0,1}}};
inline constexpr std::array<OtherTrainVehicle,6> kIntercity{{
    {IntercityLocomotive,19.4,2},{IntercityCoach,26.4,0},{IntercityCoach,26.4,0},
    {IntercityCoach,26.4,0},{IntercityCoach,26.4,0},{IntercityTailCoach,26.4,0}}};
// About 200 m keeps the whole freight inside the hidden traversal reset envelope.
inline constexpr std::array<OtherTrainVehicle,12> kFreight{{
    {FreightLocomotive,19.4,2},{ContainerFlatPair,19.6,0},{ContainerFlatLong,19.6,0},
    {ContainerFlatPair,19.6,0},{TankWagon,15.5,0},{TankWagon,15.5,0},{TankWagon,15.5,0},
    {CoveredHopper,14.0,0},{CoveredHopper,14.0,0},{OpenWagon,13.8,0},{OpenWagon,13.8,0},
    {ContainerFlatLong,19.6,0}}};
inline constexpr std::array<OtherTrainVehicle,2> kDieselUnit{{
    {DieselLeadingCar,23.0,1},{DieselTrailingCar,23.0,1}}};
} // namespace detail

[[nodiscard]] constexpr OtherTrainConsist otherTrainConsist(OtherTrainType type) noexcept
{
    switch (type)
    {
        case OtherTrainType::Intercity: return {detail::kIntercity,38.0};
        case OtherTrainType::Freight: return {detail::kFreight,25.0};
        case OtherTrainType::DieselUnit: return {detail::kDieselUnit,28.0};
        default: return {detail::kRegionalEmu,31.0};
    }
}

inline constexpr std::uint32_t kOtherTrainPhaseMask = 3U;
inline constexpr std::uint32_t kOtherTrainTypeShift = 2U;

[[nodiscard]] constexpr std::uint32_t otherTrainPhaseVariant(std::uint32_t variant) noexcept
{
    return variant & kOtherTrainPhaseMask;
}
[[nodiscard]] constexpr OtherTrainType otherTrainType(std::uint32_t variant) noexcept
{
    const std::uint32_t type = variant >> kOtherTrainTypeShift;
    return type < static_cast<std::uint32_t>(OtherTrainType::Count)
        ? static_cast<OtherTrainType>(type) : OtherTrainType::RegionalEmu;
}
[[nodiscard]] constexpr std::uint32_t otherTrainVariant(std::uint32_t phase,
                                                        OtherTrainType type) noexcept
{
    return (phase & kOtherTrainPhaseMask)
        | (static_cast<std::uint32_t>(type) << kOtherTrainTypeShift);
}

// Services appear in every 37th chunk. Their ordinal along the route, not the decorative RNG,
// chooses the type, so the established pass-by fixtures (ordinals 1 and 29) stay regional units.
[[nodiscard]] constexpr OtherTrainType otherTrainTypeForOrdinal(std::int64_t ordinal) noexcept
{
    constexpr std::array<OtherTrainType,4> rotation{
        OtherTrainType::Freight, OtherTrainType::RegionalEmu,
        OtherTrainType::Intercity, OtherTrainType::DieselUnit};
    const std::int64_t slot = ((ordinal % 4) + 4) % 4;
    return rotation[static_cast<std::size_t>(slot)];
}

} // namespace TrainSim::World

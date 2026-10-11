#pragma once

#include <optional>

namespace TrainSim::Sim {

class TrainInterior
{
public:
    static constexpr int PassengerCars = 6;
    static constexpr double CarLength = 21.0;
    static constexpr double FirstCarCentre = -52.5;
    static constexpr double PassengerMinimumZ = -62.6;
    static constexpr double PassengerMaximumZ = 62.6;
    // Render-space rail head is near y=0. The low-floor body is raised so its 8 cm local floor
    // surface exactly meets the authored 0.78 m station platform surface.
    static constexpr double FloorElevationAboveRail = 0.70;
    static constexpr double FloorSurfaceLocalY = 0.08;
    static constexpr double PlatformSurfaceAboveRail = 0.78;
    static constexpr double TrainBodyHalfWidth = 1.50;
    static constexpr double PlatformCentrelineFromTrack = 4.20;
    static constexpr double PlatformWidth = 5.00;
    static constexpr double PlatformInnerEdgeFromTrack =
        PlatformCentrelineFromTrack - PlatformWidth * 0.5;
    static constexpr double HorizontalPlatformGap =
        PlatformInnerEdgeFromTrack - TrainBodyHalfWidth;
    static constexpr double BoardingStepInnerEdge = 1.48;
    static constexpr double BoardingStepTravel = 0.24;
    static constexpr double BoardingStepPlatformOverlap =
        BoardingStepInnerEdge + BoardingStepTravel - PlatformInnerEdgeFromTrack;
    static constexpr double PassengerDoorAnimationSeconds = 1.35;
    static constexpr double PassengerDoorPassageSeconds = 1.45;
    static constexpr double PassengerDoorOffsetFromCarCentre = 8.0;
    static constexpr double PassengerDoorHalfOpening = 0.78;
    static constexpr int WindowBaysPerCar = 10;
    static constexpr double FirstWindowBeginOffset = -10.38;
    static constexpr double WindowBayPitch = 2.05;
    static constexpr double WindowClearLength = 1.81;
    static constexpr double WindowMullionWidth = WindowBayPitch - WindowClearLength;
    static constexpr int SeatRowsPerCar = 8;
    static constexpr int FirstSeatWindowBay = 1;
    static constexpr int TableBaysPerCar = 3;
    static constexpr int GangwayCount = PassengerCars - 1;
    static constexpr double FirstGangwayZ = -42.0;
    static constexpr double GangwaySensorRadius = 2.8;
    static constexpr double VestibuleFloorHalfLength = 3.0;
    static constexpr double GangwayFloorHalfLength = 0.85;
    static constexpr int ToiletCar = 3;
    static constexpr double ToiletRackClearanceBeginZ = 3.34;
    static constexpr double ToiletRackClearanceEndZ = 8.21;

    [[nodiscard]] static constexpr double gangwayZ(int index) noexcept
    {
        return FirstGangwayZ + static_cast<double>(index) * CarLength;
    }

    [[nodiscard]] static constexpr double carCentre(int car) noexcept
    {
        return FirstCarCentre + static_cast<double>(car) * CarLength;
    }

    [[nodiscard]] static constexpr double passengerDoorZ(int car) noexcept
    {
        return carCentre(car) + PassengerDoorOffsetFromCarCentre;
    }

    [[nodiscard]] static constexpr double windowClearBeginZ(int car, int windowBay) noexcept
    {
        return carCentre(car) + FirstWindowBeginOffset
            + static_cast<double>(windowBay) * WindowBayPitch;
    }

    [[nodiscard]] static constexpr double windowClearCentreZ(int car, int windowBay) noexcept
    {
        return windowClearBeginZ(car, windowBay) + WindowClearLength * 0.5;
    }

    [[nodiscard]] static constexpr double windowMullionZ(int car, int pillar) noexcept
    {
        return windowClearBeginZ(car,pillar) - WindowMullionWidth * 0.5;
    }

    [[nodiscard]] static constexpr double seatRowZ(int car, int row) noexcept
    {
        return windowClearCentreZ(car, FirstSeatWindowBay + row);
    }

    [[nodiscard]] static constexpr double tableBayZ(int car, int table) noexcept
    {
        const int firstFacingRow = table * 2 + 1;
        return (seatRowZ(car,firstFacingRow) + seatRowZ(car,firstFacingRow+1)) * 0.5;
    }

    [[nodiscard]] static constexpr bool seatSidePresent(int car, int row, int side) noexcept
    {
        return !(car == ToiletCar && row < 3 && side > 0);
    }

    [[nodiscard]] static constexpr bool overheadRackPositionAllowed(
        int car, int side, double z) noexcept
    {
        return !(car == ToiletCar && side > 0
            && z > ToiletRackClearanceBeginZ && z < ToiletRackClearanceEndZ);
    }

    [[nodiscard]] static constexpr bool innerSeatPresent(int car, int row) noexcept
    {
        return !(car == 2 && row < 2);
    }

    [[nodiscard]] static constexpr bool tableSidePresent(int car, int table, int side) noexcept
    {
        if (car < 0 || car >= PassengerCars || table < 0 || table >= TableBaysPerCar
            || (side != -1 && side != 1)) return false;
        const int firstFacingRow = table * 2 + 1;
        // The first bay in car 3 is the deliberately unobstructed wheelchair/companion zone.
        return !(car == 2 && table == 0)
            && seatSidePresent(car,firstFacingRow,side)
            && seatSidePresent(car,firstFacingRow+1,side);
    }

    [[nodiscard]] static constexpr bool insideToilet(double x, double z) noexcept
    {
        return x > 0.44 && x < 1.18 && z > 4.55 && z < 7.72;
    }

    // An open player-operated door gives a nearby player time to enter/leave;
    // walking away releases this priority even if the door remains open.
    [[nodiscard]] static bool toiletAvailableToPassengers(
        double playerX, double playerZ, bool playerDoorOpen) noexcept;

    [[nodiscard]] static std::optional<int>
    informationDisplayCar(double z, double radius = 0.65) noexcept;
    [[nodiscard]] static bool gangwaySensorContains(int index, double z) noexcept;
    // Vestibules, gangway bridge plates, and the accessible toilet use resilient flooring rather
    // than saloon carpet. Audio consumes the same authored positions as the physical interior.
    [[nodiscard]] static bool nonCarpetedFloorAt(double x, double z) noexcept;
    [[nodiscard]] static int seatRowIndex(int car, double z) noexcept;
    [[nodiscard]] static bool interactionTargetVisible(
        double playerX, double playerZ, double playerYaw,
        double targetX, double targetZ, double maximumDistance = 1.45) noexcept;
    [[nodiscard]] static bool canOccupy(double x, double z, bool toiletDoorOpen) noexcept;
};

} // namespace TrainSim::Sim

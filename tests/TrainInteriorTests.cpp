#include "TestHarness.hpp"

#include "trainsim/sim/TrainInterior.hpp"

#include <array>
#include <cmath>
#include <limits>

using TrainSim::Sim::TrainInterior;

TEST_CASE("window mullions join the shared glazing layout without longitudinal holes")
{
    for (int car=0;car<TrainInterior::PassengerCars;++car)
        for (int bay=0;bay<TrainInterior::WindowBaysPerCar;++bay)
        {
            const double begin=TrainInterior::windowClearBeginZ(car,bay);
            const double end=begin+TrainInterior::WindowClearLength;
            REQUIRE(std::abs(TrainInterior::windowMullionZ(car,bay)
                +TrainInterior::WindowMullionWidth*0.5-begin)<1e-9);
            REQUIRE(std::abs(TrainInterior::windowMullionZ(car,bay+1)
                -TrainInterior::WindowMullionWidth*0.5-end)<1e-9);
        }
}

TEST_CASE("low-floor train and station platform surfaces align")
{
    REQUIRE(std::abs(TrainInterior::FloorElevationAboveRail
                     + TrainInterior::FloorSurfaceLocalY
                     - TrainInterior::PlatformSurfaceAboveRail) < 1e-9);
    REQUIRE(std::abs(TrainInterior::HorizontalPlatformGap - 0.20) < 1e-9);
    REQUIRE(TrainInterior::HorizontalPlatformGap >= 0.15);
    REQUIRE(TrainInterior::HorizontalPlatformGap <= 0.30);
    REQUIRE(std::abs(TrainInterior::BoardingStepPlatformOverlap - 0.02) < 1e-9);
    REQUIRE(TrainInterior::BoardingStepPlatformOverlap > 0.0);
    REQUIRE(TrainInterior::PassengerDoorPassageSeconds
            > TrainInterior::PassengerDoorAnimationSeconds);
}

TEST_CASE("central aisle and gangways connect every passenger car")
{
    for (double z = TrainInterior::PassengerMinimumZ; z <= TrainInterior::PassengerMaximumZ; z += 0.10)
        REQUIRE(TrainInterior::canOccupy(0.0, z, false));

    for (int index = 0; index < TrainInterior::GangwayCount; ++index)
    {
        const double boundary = TrainInterior::gangwayZ(index);
        REQUIRE(TrainInterior::canOccupy(-0.40, boundary, false));
        REQUIRE(TrainInterior::canOccupy(0.40, boundary, false));
    }
    REQUIRE(TrainInterior::gangwayZ(0) == -42.0);
    REQUIRE(TrainInterior::gangwayZ(TrainInterior::GangwayCount - 1) == 42.0);
}

TEST_CASE("each passenger vestibule has one consistently spaced door centre")
{
    for (int car = 0; car < TrainInterior::PassengerCars; ++car)
    {
        const double door = TrainInterior::passengerDoorZ(car);
        REQUIRE(std::abs(door - TrainInterior::carCentre(car)
                         - TrainInterior::PassengerDoorOffsetFromCarCentre) < 1e-9);
        REQUIRE(door > TrainInterior::carCentre(car) - TrainInterior::CarLength * 0.5);
        REQUIRE(door < TrainInterior::carCentre(car) + TrainInterior::CarLength * 0.5);
        REQUIRE(TrainInterior::canOccupy(0.0,door,false));
        if (car > 0)
            REQUIRE(std::abs(door - TrainInterior::passengerDoorZ(car-1)
                             - TrainInterior::CarLength) < 1e-9);
    }
}

TEST_CASE("footstep surfaces follow vestibules gangways and the accessible toilet")
{
    for (int car = 0; car < TrainInterior::PassengerCars; ++car)
    {
        const double door = TrainInterior::passengerDoorZ(car);
        REQUIRE(TrainInterior::nonCarpetedFloorAt(0.0,door));
        REQUIRE(TrainInterior::nonCarpetedFloorAt(
            0.0,door+TrainInterior::VestibuleFloorHalfLength));
        REQUIRE(!TrainInterior::nonCarpetedFloorAt(
            0.0,TrainInterior::carCentre(car)));
    }
    for (int index = 0; index < TrainInterior::GangwayCount; ++index)
        REQUIRE(TrainInterior::nonCarpetedFloorAt(0.0,TrainInterior::gangwayZ(index)));
    REQUIRE(TrainInterior::nonCarpetedFloorAt(0.66,5.70));
    REQUIRE(!TrainInterior::nonCarpetedFloorAt(-0.66,5.70));
    REQUIRE(!TrainInterior::nonCarpetedFloorAt(
        0.0,std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("gangway sensors cover each doorway and reject invalid positions")
{
    for (int index = 0; index < TrainInterior::GangwayCount; ++index)
    {
        const double centre = TrainInterior::gangwayZ(index);
        REQUIRE(TrainInterior::gangwaySensorContains(index, centre));
        REQUIRE(TrainInterior::gangwaySensorContains(
            index, centre + TrainInterior::GangwaySensorRadius - 0.01));
        REQUIRE(!TrainInterior::gangwaySensorContains(
            index, centre + TrainInterior::GangwaySensorRadius + 0.01));
    }
    REQUIRE(!TrainInterior::gangwaySensorContains(-1, 0.0));
    REQUIRE(!TrainInterior::gangwaySensorContains(TrainInterior::GangwayCount, 0.0));
    REQUIRE(!TrainInterior::gangwaySensorContains(
        0, std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("context interactions require a nearby target in the player's view")
{
    constexpr double halfPi = 1.5707963267948966;
    REQUIRE(TrainInterior::interactionTargetVisible(0.0, 0.0, 0.0, 0.0, -1.0));
    REQUIRE(!TrainInterior::interactionTargetVisible(0.0, 0.0, 0.0, 0.0, 1.0));
    REQUIRE(!TrainInterior::interactionTargetVisible(0.0, 0.0, 0.0, 1.0, 0.0));
    REQUIRE(TrainInterior::interactionTargetVisible(0.0, 0.0, halfPi, 1.0, 0.0));
    REQUIRE(TrainInterior::interactionTargetVisible(0.0, 0.0, 0.0, 0.0, 0.0));
    REQUIRE(!TrainInterior::interactionTargetVisible(0.0, 0.0, 0.0, 0.0, -1.46));
    REQUIRE(!TrainInterior::interactionTargetVisible(
        0.0, 0.0, std::numeric_limits<double>::quiet_NaN(), 0.0, -1.0));
}

TEST_CASE("every passenger seat row is centred on a clear window bay")
{
    for (int car = 0; car < TrainInterior::PassengerCars; ++car)
        for (int row = 0; row < TrainInterior::SeatRowsPerCar; ++row)
        {
            const int windowBay = TrainInterior::FirstSeatWindowBay + row;
            const double begin = TrainInterior::windowClearBeginZ(car, windowBay);
            const double end = begin + TrainInterior::WindowClearLength;
            const double seat = TrainInterior::seatRowZ(car, row);
            REQUIRE(std::abs(seat - (begin + end) * 0.5) < 1e-9);
            REQUIRE(seat - begin > 0.90);
            REQUIRE(end - seat > 0.90);
            REQUIRE(TrainInterior::seatRowIndex(car, seat) == row);
        }
    REQUIRE(TrainInterior::seatRowIndex(-1, 0.0) == -1);
    REQUIRE(TrainInterior::seatRowIndex(
        0, std::numeric_limits<double>::quiet_NaN()) == -1);
}

TEST_CASE("the accessible toilet interrupts only its overhead rack")
{
    REQUIRE(!TrainInterior::overheadRackPositionAllowed(
        TrainInterior::ToiletCar,1,6.0));
    REQUIRE(TrainInterior::overheadRackPositionAllowed(
        TrainInterior::ToiletCar,-1,6.0));
    REQUIRE(TrainInterior::overheadRackPositionAllowed(
        TrainInterior::ToiletCar,1,TrainInterior::ToiletRackClearanceBeginZ));
    REQUIRE(TrainInterior::overheadRackPositionAllowed(
        TrainInterior::ToiletCar,1,TrainInterior::ToiletRackClearanceEndZ));
    REQUIRE(TrainInterior::overheadRackPositionAllowed(2,1,6.0));
}

TEST_CASE("seat luggage wall and cab geometry block the player")
{
    REQUIRE(!TrainInterior::canOccupy(1.0, -59.5, false));
    REQUIRE(!TrainInterior::canOccupy(-1.0, -61.0, false));
    REQUIRE(!TrainInterior::canOccupy(1.30, 0.0, false));
    REQUIRE(!TrainInterior::canOccupy(0.0, -63.0, false));
    REQUIRE(!TrainInterior::canOccupy(0.0, 63.0, false));
    REQUIRE(TrainInterior::canOccupy(0.0, -62.5, false));
    REQUIRE(TrainInterior::canOccupy(0.0, 62.5, false));
}

TEST_CASE("toilet is reachable only through its open doorway")
{
    REQUIRE(!TrainInterior::canOccupy(0.55, 4.07, false));
    REQUIRE(TrainInterior::canOccupy(0.55, 4.07, true));
    REQUIRE(TrainInterior::canOccupy(0.66, 5.70, true));
    REQUIRE(!TrainInterior::canOccupy(0.40, 5.70, true));
    REQUIRE(!TrainInterior::canOccupy(1.22, 5.70, true));
    REQUIRE(!TrainInterior::canOccupy(0.66, 7.90, true));
    REQUIRE(TrainInterior::insideToilet(0.66, 5.70));
    REQUIRE(!TrainInterior::insideToilet(0.34, 5.70));
    REQUIRE(!TrainInterior::insideToilet(0.66, 4.20));
}

TEST_CASE("opening the toilet protects nearby entry and releases priority when walking away")
{
    REQUIRE(TrainInterior::toiletAvailableToPassengers(-0.95,4.1,false));
    REQUIRE(!TrainInterior::toiletAvailableToPassengers(-0.95,4.1,true));
    REQUIRE(!TrainInterior::toiletAvailableToPassengers(0.64,4.537,true));
    REQUIRE(!TrainInterior::toiletAvailableToPassengers(0.66,5.70,false));
    REQUIRE(!TrainInterior::toiletAvailableToPassengers(0.66,5.70,true));
    REQUIRE(TrainInterior::toiletAvailableToPassengers(0.0,1.5,true));
}

TEST_CASE("accessible rows preserve additional aisle-side floor space")
{
    constexpr double accessibleRowZ = TrainInterior::seatRowZ(2, 0);
    REQUIRE(TrainInterior::canOccupy(0.60, accessibleRowZ, false));
    REQUIRE(TrainInterior::canOccupy(-0.60, accessibleRowZ, false));
    REQUIRE(!TrainInterior::canOccupy(1.02, accessibleRowZ, false));
    REQUIRE(!TrainInterior::canOccupy(-1.02, accessibleRowZ, false));
}

TEST_CASE("facing-seat tables have physical footprints without narrowing the aisle")
{
    for (int car = 0; car < TrainInterior::PassengerCars; ++car)
        for (int table = 0; table < TrainInterior::TableBaysPerCar; ++table)
        {
            const double z = TrainInterior::tableBayZ(car,table);
            REQUIRE(TrainInterior::canOccupy(0.0,z,false));
            for (int side : {-1,1})
            {
                const bool present = TrainInterior::tableSidePresent(car,table,side);
                if (present)
                {
                    REQUIRE(TrainInterior::canOccupy(0.44*side,z,false));
                    REQUIRE(!TrainInterior::canOccupy(0.78*side,z,false));
                }
            }
        }

    REQUIRE(!TrainInterior::tableSidePresent(2,0,-1));
    REQUIRE(!TrainInterior::tableSidePresent(2,0,1));
    REQUIRE(!TrainInterior::tableSidePresent(TrainInterior::ToiletCar,0,1));
    REQUIRE(TrainInterior::tableSidePresent(TrainInterior::ToiletCar,0,-1));
    REQUIRE(!TrainInterior::tableSidePresent(-1,0,1));
    REQUIRE(!TrainInterior::tableSidePresent(0,TrainInterior::TableBaysPerCar,1));
}

TEST_CASE("each overhead passenger display has a narrow unambiguous interaction zone")
{
    for (int car = 0; car < TrainInterior::PassengerCars; ++car)
    {
        const double centre = TrainInterior::FirstCarCentre
            + static_cast<double>(car) * TrainInterior::CarLength;
        REQUIRE(TrainInterior::informationDisplayCar(centre) == car);
        REQUIRE(TrainInterior::informationDisplayCar(centre + 0.64) == car);
        REQUIRE(!TrainInterior::informationDisplayCar(centre + 0.66).has_value());
    }
    REQUIRE(!TrainInterior::informationDisplayCar(0.0, -1.0).has_value());
}

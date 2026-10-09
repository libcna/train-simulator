#include "TestHarness.hpp"

#include "trainsim/render/AvatarMotion.hpp"
#include "trainsim/render/AvatarAppearance.hpp"
#include "trainsim/render/PlatformLayout.hpp"
#include <array>

TEST_CASE("civilian selection favours uncovered hair and cannot offset conspicuous hats")
{
    using TrainSim::Render::civilianAppearanceScoreBytes;
    std::array<std::uint8_t,1021> bytes{};
    bytes[0]=1; bytes[1]='C'; bytes[2]='N'; bytes[3]='A'; bytes[8]=3;
    bytes[35]=40;
    const int uncovered=civilianAppearanceScoreBytes(bytes);
    REQUIRE(uncovered>=21);
    bytes[41]=100;
    REQUIRE(civilianAppearanceScoreBytes(bytes)<uncovered);
    REQUIRE(civilianAppearanceScoreBytes(bytes)>=21);
    bytes[28]=250; bytes[29]=210; bytes[30]=60;
    REQUIRE(civilianAppearanceScoreBytes(bytes)<0);
    bytes[28]=bytes[29]=bytes[30]=80;
    bytes[41]=102;
    REQUIRE(civilianAppearanceScoreBytes(bytes)<0);
    bytes[41]=0; bytes[35]=42;
    REQUIRE(civilianAppearanceScoreBytes(bytes)<0);
    bytes[8]=4;
    REQUIRE(civilianAppearanceScoreBytes(bytes)==0);
    bytes[8]=3; bytes[0]=3;
    REQUIRE(civilianAppearanceScoreBytes(bytes)==0);
    bytes[0]=1; bytes[1]='X';
    REQUIRE(civilianAppearanceScoreBytes(bytes)==0);
    REQUIRE(civilianAppearanceScoreBytes({})==0);
}

TEST_CASE("civilian wardrobe cannot trade saturated clothing for uncovered hair")
{
    using TrainSim::Render::civilianAppearanceScoreBytes;
    std::array<std::uint8_t,1021> bytes{};
    bytes[0]=1; bytes[1]='C'; bytes[2]='N'; bytes[3]='A'; bytes[8]=3;
    bytes[35]=40;
    for (const auto colour : {std::array<unsigned char,3>{240,240,236},
                              {60,62,70}, {120,90,60}, {40,110,110}, {130,80,170}})
    {
        for (std::size_t channel=0; channel<3; ++channel) bytes[19+channel]=colour[channel];
        REQUIRE(civilianAppearanceScoreBytes(bytes)>=29);
    }
    for (const auto colour : {std::array<unsigned char,3>{250,210,60},
                              {240,150,40}, {220,60,56}, {60,80,180}})
    {
        for (std::size_t channel=0; channel<3; ++channel) bytes[19+channel]=colour[channel];
        REQUIRE(civilianAppearanceScoreBytes(bytes)<21);
    }
    bytes[19]=bytes[20]=bytes[21]=80;
    bytes[22]=250; bytes[23]=210; bytes[24]=60;
    REQUIRE(civilianAppearanceScoreBytes(bytes)<21);
}

TEST_CASE("seated wardrobe filter is limited to known CNA skirt descriptions")
{
    std::array<std::uint8_t,1021> bytes{};
    bytes[0]=1; bytes[1]='C'; bytes[2]='N'; bytes[3]='A'; bytes[8]=3;
    for (unsigned id : {40U,41U,42U,43U,44U,45U})
    {
        bytes[35]=static_cast<std::uint8_t>(id);
        REQUIRE(TrainSim::Render::avatarHasSeatedSkirtRisk(bytes) == (id == 42 || id == 45));
    }
    bytes[0]=2;
    REQUIRE(TrainSim::Render::avatarHasSeatedSkirtRisk(bytes));
    bytes[8]=4;
    REQUIRE(!TrainSim::Render::avatarHasSeatedSkirtRisk(bytes));
    bytes[8]=3; bytes[0]=3;
    REQUIRE(!TrainSim::Render::avatarHasSeatedSkirtRisk(bytes));
    bytes[0]=1; bytes[36]=1;
    REQUIRE(!TrainSim::Render::avatarHasSeatedSkirtRisk(bytes));
    REQUIRE(!TrainSim::Render::avatarHasSeatedSkirtRisk({}));
}

TEST_CASE("platform waiting groups clear fixtures across station families")
{
    using namespace TrainSim::Render;
    for (bool terminal : {false,true})
        for (bool major : {false,true})
        {
            const auto positions = platformWaitingOffsets(terminal,major);
            REQUIRE(positions == platformWaitingOffsets(terminal,major));
            for (std::size_t slot = 0; slot < positions.size(); ++slot)
                REQUIRE(platformWaitingClear(positions[slot],platformWaitingLaterals[slot],terminal,major));
            REQUIRE(std::abs(positions[5]-positions[4]-1.6) < 1e-8);
            REQUIRE(platformConversationClear(positions[4],platformWaitingLaterals[4],
                positions[5],platformWaitingLaterals[5],terminal,major));
        }
    REQUIRE(!platformWaitingClear(34.6,4.99,true,true));
    REQUIRE(!platformWaitingClear(38.0,4.65,true,true));
    REQUIRE(!platformWaitingClear(7.0,4.75,false,false));
}

TEST_CASE("compact station name boards leave public building entrances clear")
{
    using namespace TrainSim::Render;
    for (bool major : {false,true})
        for (double variant : {-1.0,1.0})
        {
            const double length=major ? 42.0 : 16.0;
            const double door=-(major ? 28.0 : 12.0)+variant*0.07*length;
            for (double sign : stationNameBoardOffsets(major))
                REQUIRE(std::abs(sign-door)>stationNameBoardHalfWidth+0.81+0.10);
        }
}

TEST_CASE("canopy frames clear entrance variants and lamps stay between structural bays")
{
    using namespace TrainSim::Render;
    for (bool terminal : {false,true})
        for (bool major : {false,true})
        {
            const float length=platformCanopyLength(terminal,major);
            const int lights=std::max(3,static_cast<int>(length/18));
            const double centre=terminal ? 42 : major ? 28 : 12;
            const double buildingLength=terminal ? 58 : major ? 42 : 16;
            double previous=-1000;
            for (double nominal=-length*0.42;nominal<=length*0.42;nominal+=canopySupportSpacing)
            {
                const double post=canopySupportOffset(nominal,terminal,major);
                REQUIRE(std::abs(post-platformIdentifierOffset)>0.6);
                REQUIRE(std::abs(post+8.5)>=1.699);
                REQUIRE(std::abs(post-platformDisplayOffset(terminal,major))>1.70);
                if (previous>-999) REQUIRE(post-previous>3.5 && post-previous<8.3);
                previous=post;
                for (double orientation : {-1.0,1.0})
                    for (double variant : {-1.0,1.0})
                        if (terminal || orientation<0)
                            REQUIRE(std::abs(post-(orientation*centre+variant*buildingLength*0.07))>=1.099);
                for (int light=0;light<lights;++light)
                    REQUIRE(std::abs(post-canopyLightOffset(length,light,lights))>1.0);
            }
        }
}

TEST_CASE("live platform display fits below canopy beams with supported readable rows")
{
    using namespace TrainSim::Render;
    REQUIRE(platformDisplayCentreHeight+platformDisplayHalfHeight+0.04f<3.20f);
    REQUIRE(platformDisplayPostTop>platformDisplayCentreHeight-platformDisplayHalfHeight);
    REQUIRE(platformDisplayFirstRow<platformDisplayCentreHeight+platformDisplayHalfHeight-0.04f);
    REQUIRE(platformDisplayFirstRow-0.64f-0.28f>
            platformDisplayCentreHeight-platformDisplayHalfHeight+0.04f);
    for (bool terminal : {false,true})
        for (bool major : {false,true})
            for (double post : {-1.18,1.18})
                REQUIRE(!platformWaitingClear(platformDisplayOffset(terminal,major)+post,
                                              6.20,terminal,major));
}

TEST_CASE("rear platform fencing has explicit supported run ends around the access opening")
{
    using namespace TrainSim::Render;
    for (double length : {180.0,240.0})
        for (double access : {-42.0,-28.0,-12.0,42.0})
        {
            const auto runs=platformFenceRuns(length,access);
            REQUIRE(runs[0][0]==-length*0.5+3);
            REQUIRE(runs[1][1]==length*0.5-3);
            REQUIRE(runs[0][1]==access-5.2);
            REQUIRE(runs[1][0]==access+5.2);
            for (const auto& run : runs)
            {
                const int bays=static_cast<int>(std::ceil((run[1]-run[0])/6));
                const double step=(run[1]-run[0])/bays;
                REQUIRE(step>0 && step<=6);
                REQUIRE(std::abs((run[0]+bays*step)-run[1])<1e-8);
            }
        }
}

TEST_CASE("platform conversation clearance detects a post between clear endpoints")
{
    using namespace TrainSim::Render;
    REQUIRE(platformWaitingClear(33.5,4.65,true,true));
    REQUIRE(platformWaitingClear(35.2,4.99,true,true));
    REQUIRE(!platformConversationClear(33.5,4.65,35.2,4.99,true,true));
    REQUIRE(!platformConversationClear(35.2,4.99,33.5,4.65,true,true));
    REQUIRE(platformConversationClear(31.0,4.65,32.6,4.99,true,true));
    REQUIRE(!platformConversationClear(34.56,4.8,34.56,4.8,true,true));
    REQUIRE(platformConversationClear(32.0,4.8,32.0,4.8,true,true));
}

TEST_CASE("platform conversations require a present non-phone partner")
{
    using TrainSim::Render::platformConversationSlot;
    for (std::size_t slot = 0; slot < 6; ++slot)
    {
        REQUIRE(platformConversationSlot(slot,0,6) == (slot >= 4));
        REQUIRE(!platformConversationSlot(slot,1,4));
        REQUIRE(!platformConversationSlot(slot,2,2));
    }
}

TEST_CASE("avatar forward points toward cardinal and diagonal navigation targets")
{
    for (int x = -1; x <= 1; ++x)
        for (int z = -1; z <= 1; ++z)
        {
            if (x == 0 && z == 0) continue;
            const float yaw = TrainSim::Render::avatarFacingDirection(x,z);
            const double length = std::hypot(x,z);
            REQUIRE(std::abs(-std::sin(yaw)-x/length) < 1e-6);
            REQUIRE(std::abs(-std::cos(yaw)-z/length) < 1e-6);
        }
    REQUIRE(TrainSim::Render::avatarFacingDirection(0.0,0.0) == 0.0f);
}

TEST_CASE("window gazes preserve seat facing and turn the head toward either window")
{
    using namespace TrainSim::Render;
    for (int row = 0; row < 6; ++row)
    {
        const float facing = avatarSeatFacing(row);
        const float left = avatarWindowGaze(-1.0,facing);
        const float right = avatarWindowGaze(1.0,facing);
        REQUIRE(std::abs(left) <= 0.85f);
        REQUIRE(std::abs(right) <= 0.85f);
        REQUIRE(left*right < 0.0f);
        REQUIRE((row%2 == 0 ? left > 0.0f : left < 0.0f));
        REQUIRE(std::abs(-std::cos(facing)-(row%2 == 0 ? -1.0f : 1.0f)) < 1e-5f);
        REQUIRE(avatarSeatFacing(row) == facing);
    }
}

#include <cmath>

using TrainSim::Render::advanceAvatarGait;
using TrainSim::Render::avatarSeatTransferWeight;
using TrainSim::Render::seededAvatarGaitPhase;
using TrainSim::Render::stabilizeAvatarSeatExit;

TEST_CASE("avatar gait phase follows travelled distance")
{
    const auto step = advanceAvatarGait(0.25f,0.0,1.0,0.06,1.08,true,true);
    REQUIRE(step.moving);
    REQUIRE(std::abs(step.distanceMetres-0.10f) < 0.0001f);
    REQUIRE(std::abs(step.phaseRadians-0.985f) < 0.0002f);
}

TEST_CASE("blocked and stationary avatars plant their gait")
{
    const auto blocked = advanceAvatarGait(1.2f,0.28,14.0,0.28,14.0,true,true);
    REQUIRE(!blocked.moving);
    REQUIRE(blocked.phaseRadians == 1.2f);

    const auto idleTranslation = advanceAvatarGait(2.1f,0.0,0.0,0.1,0.0,true,false);
    REQUIRE(!idleTranslation.moving);
    REQUIRE(idleTranslation.phaseRadians == 2.1f);
}

TEST_CASE("avatar gait ignores first samples and discontinuous teleports")
{
    const auto first = advanceAvatarGait(0.7f,0.0,0.0,12.0,5.0,false,true);
    REQUIRE(!first.moving);
    REQUIRE(first.phaseRadians == 0.7f);

    const auto teleport = advanceAvatarGait(0.7f,0.0,0.0,4.2,0.0,true,true);
    REQUIRE(!teleport.moving);
    REQUIRE(teleport.phaseRadians == 0.7f);
}

TEST_CASE("avatar gait seeds and wraps deterministically")
{
    const float phase = seededAvatarGaitPhase(0xA51CE55U);
    REQUIRE(phase >= 0.0f);
    REQUIRE(phase < 6.283186f);
    REQUIRE(phase == seededAvatarGaitPhase(0xA51CE55U));

    const auto wrapped = advanceAvatarGait(6.20f,0.0,0.0,0.10,0.0,true,true);
    REQUIRE(wrapped.moving);
    REQUIRE(wrapped.phaseRadians >= 0.0f);
    REQUIRE(wrapped.phaseRadians < 6.283186f);
}

TEST_CASE("avatar seat exit stays planted until the skeleton starts standing")
{
    const auto seated = stabilizeAvatarSeatExit(0.55,12.4,0.92,12.0,1.0f,true);
    REQUIRE(std::abs(seated.x-0.92) < 0.000001);
    REQUIRE(std::abs(seated.z-12.0) < 0.000001);
    REQUIRE(seated.weightTransfer == 0.0f);

    const auto transferring = stabilizeAvatarSeatExit(0.55,12.4,0.92,12.0,0.5f,true);
    REQUIRE(std::abs(transferring.x-0.735) < 0.000001);
    REQUIRE(std::abs(transferring.z-12.2) < 0.000001);
    REQUIRE(transferring.weightTransfer == 1.0f);

    const auto standing = stabilizeAvatarSeatExit(0.55,12.4,0.92,12.0,0.0f,true);
    REQUIRE(std::abs(standing.x-0.55) < 0.000001);
    REQUIRE(std::abs(standing.z-12.4) < 0.000001);
    REQUIRE(standing.weightTransfer == 0.0f);
}

TEST_CASE("avatar seat stabilization does not affect ordinary navigation")
{
    const auto walking = stabilizeAvatarSeatExit(-0.24,31.7,-0.92,30.0,0.8f,false);
    REQUIRE(walking.x == -0.24);
    REQUIRE(walking.z == 31.7);
    REQUIRE(walking.weightTransfer == 0.0f);

    const auto clamped = stabilizeAvatarSeatExit(0.2,4.0,0.9,3.5,1.7f,true);
    REQUIRE(std::abs(clamped.x-0.9) < 0.000001);
    REQUIRE(std::abs(clamped.z-3.5) < 0.000001);
}

TEST_CASE("avatar seat weight transfer supports both sitting and standing")
{
    REQUIRE(avatarSeatTransferWeight(0.0f,true) == 0.0f);
    REQUIRE(avatarSeatTransferWeight(0.5f,true) == 1.0f);
    REQUIRE(avatarSeatTransferWeight(1.0f,true) == 0.0f);
    REQUIRE(avatarSeatTransferWeight(0.5f,false) == 0.0f);
    REQUIRE(avatarSeatTransferWeight(-0.5f,true) == 0.0f);
    REQUIRE(avatarSeatTransferWeight(1.5f,true) == 0.0f);
}

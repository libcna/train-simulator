#include "TestHarness.hpp"

#include "trainsim/render/ProceduralMaterials.hpp"

#include <algorithm>
#include <cstdint>

using TrainSim::Render::makeTerrainDetailImage;
using TrainSim::Render::makeBallastDetailImage;
using TrainSim::Render::makeCarpetDetailImage;
using TrainSim::Render::makeConcreteDetailImage;
using TrainSim::Render::makeInteriorPanelDetailImage;
using TrainSim::Render::makeWaterDetailImage;
using TrainSim::Render::makeResilientFloorDetailImage;
using TrainSim::Render::makeMasonryDetailImage;
using TrainSim::Render::makeRenderedFacadeDetailImage;
using TrainSim::Render::makeCloudDetailImage;
using TrainSim::Render::makeCloudAtlasImage;

TEST_CASE("alpha-test foliage mipmaps preserve coverage without changing RGB")
{
    const auto coverage=[](const auto& image) {
        return static_cast<double>(std::count_if(image.pixels.begin(),image.pixels.end(),
            [](const auto& p) { return p.getAProperty()>96; }))/static_cast<double>(image.pixels.size());
    };
    auto level=TrainSim::Render::makeLeafSprayImage();
    const double target=coverage(level);
    while(level.width>1)
    {
        const auto average=level.downsampled();
        const auto corrected=level.downsampledForAlphaTest(96,target);
        const auto repeated=level.downsampledForAlphaTest(96,target);
        REQUIRE(std::abs(coverage(corrected)-target)<=std::abs(coverage(average)-target)+1e-9);
        if(corrected.width>=16) REQUIRE(std::abs(coverage(corrected)-target)<0.04);
        for(std::size_t i=0;i<corrected.pixels.size();++i)
        {
            REQUIRE(corrected.pixels[i]==repeated.pixels[i]);
            REQUIRE(corrected.pixels[i].getRProperty()==average.pixels[i].getRProperty());
            REQUIRE(corrected.pixels[i].getGProperty()==average.pixels[i].getGProperty());
            REQUIRE(corrected.pixels[i].getBProperty()==average.pixels[i].getBProperty());
            if(average.pixels[i].getAProperty()==0) REQUIRE(corrected.pixels[i].getAProperty()==0);
        }
        level=corrected;
    }
}

TEST_CASE("needle sprays retain transparent gaps and borders with deterministic detail")
{
    const auto a=TrainSim::Render::makeNeedleSprayImage();
    const auto b=TrainSim::Render::makeNeedleSprayImage();
    std::size_t solid=0;
    for (int y=0;y<a.height;++y)
        for (int x=0;x<a.width;++x)
        {
            const auto i=static_cast<std::size_t>(y*a.width+x);
            REQUIRE(a.pixels[i]==b.pixels[i]);
            solid+=a.pixels[i].getAProperty()>96;
            REQUIRE(a.pixels[i].getRProperty()>=222);
            if (x==0 || y==0 || x==a.width-1 || y==a.height-1)
                REQUIRE(a.pixels[i].getAProperty()==0);
        }
    REQUIRE(solid>a.pixels.size()/10 && solid<a.pixels.size()/2);
}

TEST_CASE("leaf spray cutout has deterministic leaves and transparent borders")
{
    const auto a=TrainSim::Render::makeLeafSprayImage();
    const auto b=TrainSim::Render::makeLeafSprayImage();
    std::size_t solid=0;
    for (int y=0;y<a.height;++y)
        for (int x=0;x<a.width;++x)
        {
            const auto i=static_cast<std::size_t>(y*a.width+x);
            REQUIRE(a.pixels[i]==b.pixels[i]);
            solid+=a.pixels[i].getAProperty()>96;
            REQUIRE(a.pixels[i].getRProperty()>=205);
            if(x==0 || y==0 || x==a.width-1 || y==a.height-1)
                REQUIRE(a.pixels[i].getAProperty()==0);
        }
    REQUIRE(solid>a.pixels.size()/10);
    REQUIRE(solid<a.pixels.size()/2);
}

TEST_CASE("field surface margins join continuously and preserve railway and haze colours")
{
    using TrainSim::Render::ruralTerrainSurfaceTint;
    using TrainSim::Render::ruralParcelTint;
    for (double side : {-1.0,1.0})
    {
        for (double across : {5.0,10.0,20.0,400.0,900.0})
            REQUIRE(ruralTerrainSurfaceTint(123,side*across)==ruralParcelTint(123,side*across));
        for (double boundary : {26.0,100.0,260.0})
        {
            const auto a=ruralTerrainSurfaceTint(123,side*(boundary-0.001));
            const auto b=ruralTerrainSurfaceTint(123,side*(boundary+0.001));
            REQUIRE(std::abs(a.getRProperty()-b.getRProperty())<=1);
            REQUIRE(std::abs(a.getGProperty()-b.getGProperty())<=1);
            REQUIRE(std::abs(a.getBProperty()-b.getBProperty())<=1);
        }
        for (std::int64_t parcel : {1,104,572})
        {
            const double distance=TrainSim::Render::ruralParcelBoundary(parcel,side);
            const auto a=ruralTerrainSurfaceTint(distance-0.001,side*70);
            const auto b=ruralTerrainSurfaceTint(distance+0.001,side*70);
            REQUIRE(a==b);
            REQUIRE(a.getGProperty()==133);
        }
        REQUIRE(ruralTerrainSurfaceTint(123,side*70)==ruralTerrainSurfaceTint(123,side*70));
        REQUIRE(ruralTerrainSurfaceTint(123,side*70)!=ruralTerrainSurfaceTint(173,side*70));
    }
}

TEST_CASE("rural parcels share field colours without tinting the railway or distant haze")
{
    using TrainSim::Render::ruralParcelTint;
    std::vector<unsigned> identities;
    for (int parcel=0;parcel<24;++parcel)
    {
        for (double side : {-1.0,1.0})
        {
            const double distance=TrainSim::Render::ruralParcelBoundary(parcel,side);
            const double end=TrainSim::Render::ruralParcelBoundary(parcel+1,side);
            const auto colour=ruralParcelTint(distance+1,side*35).getPackedValueProperty();
            REQUIRE(colour==ruralParcelTint(end-1,side*90).getPackedValueProperty());
            REQUIRE(ruralParcelTint(distance+1,side*120).getPackedValueProperty()
                ==ruralParcelTint(end-1,side*250).getPackedValueProperty());
            if (std::find(identities.begin(),identities.end(),colour)==identities.end())
                identities.push_back(colour);
            REQUIRE(ruralParcelTint(distance,side*10).getGProperty()==96);
            REQUIRE(ruralParcelTint(distance,side*20).getGProperty()==133);
            REQUIRE(ruralParcelTint(distance,side*400).getGProperty()==119);
            REQUIRE(ruralParcelTint(distance,side*900).getGProperty()==124);
        }
    }
    REQUIRE(identities.size()==6);
}

TEST_CASE("field parcels vary their length per side on whole terrain cells")
{
    using TrainSim::Render::ruralParcelBoundary;
    using TrainSim::Render::ruralParcelIndex;
    int differentSides=0,shortParcels=0,longParcels=0;
    for (std::int64_t parcel=0;parcel<600;++parcel)
        for (double side : {-1.0,1.0})
        {
            const double begin=ruralParcelBoundary(parcel,side);
            const double end=ruralParcelBoundary(parcel+1,side);
            REQUIRE(std::abs(begin/50.0-std::round(begin/50.0))<1e-9);
            REQUIRE(end-begin>=200.0);
            REQUIRE(end-begin<=800.0);
            if (end-begin<450.0) ++shortParcels;
            if (end-begin>550.0) ++longParcels;
            REQUIRE(ruralParcelIndex(begin,side)==parcel);
            REQUIRE(ruralParcelIndex(begin+0.5*(end-begin),side)==parcel);
            REQUIRE(ruralParcelIndex(end-0.001,side)==parcel);
            if (side>0 && begin!=ruralParcelBoundary(parcel,-1.0)) ++differentSides;
        }
    REQUIRE(differentSides>400);
    REQUIRE(shortParcels>100);
    REQUIRE(longParcels>100);
}

TEST_CASE("terminal terrain zoning preserves track and distant urban colours")
{
    using TrainSim::Render::terminalGroundTint;
    using TrainSim::Render::urbanGroundTint;
    constexpr double length=286400;
    for (double d : {-700.0,700.0,52000.0,length-700,length+700})
        for (double x : {-100.0,20.0,60.0})
            REQUIRE(terminalGroundTint(d,x,length).getPackedValueProperty()
                    == urbanGroundTint(d,x).getPackedValueProperty());
    for (double d : {-250.0,0.0,250.0,length-250,length,length+250})
    {
        for (double x : {-300.0,-9.0,0.0,9.0,300.0})
            REQUIRE(terminalGroundTint(d,x,length).getPackedValueProperty()
                    == urbanGroundTint(d,x).getPackedValueProperty());
        for (double x : {-60.0,-26.0,26.0,60.0})
        {
            const auto a=terminalGroundTint(d,x,length);
            const auto b=terminalGroundTint(d+0.01,x,length);
            REQUIRE(a.getAProperty()==255);
            REQUIRE(std::abs(static_cast<int>(a.getRProperty())-b.getRProperty())<=1);
            REQUIRE(std::abs(static_cast<int>(a.getGProperty())-b.getGProperty())<=1);
            REQUIRE(std::abs(static_cast<int>(a.getBProperty())-b.getBProperty())<=1);
        }
    }
    const auto gravel=terminalGroundTint(0,26,length),lawn=terminalGroundTint(0,60,length);
    REQUIRE(gravel.getRProperty()>lawn.getRProperty()+20);
    REQUIRE(lawn.getGProperty()>lawn.getRProperty()+20);
}

TEST_CASE("upholstery has a distinct deterministic opaque woven source")
{
    const auto a=TrainSim::Render::makeUpholsteryDetailImage();
    const auto b=TrainSim::Render::makeUpholsteryDetailImage();
    const auto carpet=makeCarpetDetailImage();
    int changed=0, minimum=255, maximum=0;
    REQUIRE(a.pixels.size() == 256*256);
    for (std::size_t i=0;i<a.pixels.size();++i)
    {
        const auto& p=a.pixels[i];
        REQUIRE(p.getPackedValueProperty() == b.pixels[i].getPackedValueProperty());
        REQUIRE(p.getAProperty() == 255);
        REQUIRE(p.getRProperty() == p.getGProperty());
        REQUIRE(p.getRProperty() == p.getBProperty());
        minimum=std::min(minimum,static_cast<int>(p.getRProperty()));
        maximum=std::max(maximum,static_cast<int>(p.getRProperty()));
        changed += p.getPackedValueProperty() != carpet.pixels[i%carpet.pixels.size()].getPackedValueProperty();
    }
    REQUIRE(changed > 48000);
    REQUIRE(minimum >= 150);
    REQUIRE(maximum-minimum > 70);
    auto mip=a;
    while (mip.width>1) mip=mip.downsampled();
    REQUIRE(mip.pixels[0].getAProperty() == 255);
    REQUIRE(mip.pixels[0].getRProperty() > 210);
}

TEST_CASE("moquette lower resolutions filter the native weave without changing its design")
{
    auto mip=TrainSim::Render::makeUpholsteryDetailImage();
    while (mip.width>8)
    {
        mip=mip.downsampled();
        const auto generated=TrainSim::Render::makeUpholsteryDetailImage(mip.width);
        REQUIRE(generated.pixels.size()==mip.pixels.size());
        for (std::size_t i=0;i<mip.pixels.size();++i)
            REQUIRE(generated.pixels[i].getPackedValueProperty()==mip.pixels[i].getPackedValueProperty());
    }
    // At a 4 cm shift the visible motif must not simply repeat as the old dot grid did.
    const auto full=TrainSim::Render::makeUpholsteryDetailImage();
    int differing=0;
    for (int y=0;y<256;++y)
        for (int x=0;x<256;++x)
            if (std::abs(static_cast<int>(full.pixels[y*256+x].getRProperty())
                -static_cast<int>(full.pixels[y*256+(x+64)%256].getRProperty()))>18) ++differing;
    REQUIRE(differing>300);
}

TEST_CASE("roof tiles have deterministic staggered courses and opaque mip levels")
{
    const auto a=TrainSim::Render::makeRoofDetailImage();
    const auto b=TrainSim::Render::makeRoofDetailImage();
    int minimum=255,maximum=0;
    for (std::size_t i=0;i<a.pixels.size();++i)
    {
        REQUIRE(a.pixels[i].getPackedValueProperty()==b.pixels[i].getPackedValueProperty());
        REQUIRE(a.pixels[i].getAProperty()==255);
        minimum=std::min(minimum,static_cast<int>(a.pixels[i].getRProperty()));
        maximum=std::max(maximum,static_cast<int>(a.pixels[i].getRProperty()));
    }
    REQUIRE(minimum>=170);
    REQUIRE(maximum-minimum>40);
    auto mip=a;
    while (mip.width>1) mip=mip.downsampled();
    REQUIRE(mip.pixels[0].getAProperty()==255);
}

TEST_CASE("foliage material has opaque deterministic leaf detail and a complete mip chain")
{
    const auto a=TrainSim::Render::makeFoliageDetailImage();
    const auto b=TrainSim::Render::makeFoliageDetailImage();
    REQUIRE(a.width == 128);
    REQUIRE(a.pixels.size() == 128*128);
    int minimum=255, maximum=0;
    for (std::size_t i=0;i<a.pixels.size();++i)
    {
        const auto& p=a.pixels[i];
        REQUIRE(p.getPackedValueProperty() == b.pixels[i].getPackedValueProperty());
        REQUIRE(p.getAProperty() == 255);
        REQUIRE(p.getRProperty() == p.getGProperty());
        minimum=std::min(minimum,static_cast<int>(p.getRProperty()));
        maximum=std::max(maximum,static_cast<int>(p.getRProperty()));
    }
    REQUIRE(minimum >= 180);
    REQUIRE(maximum-minimum >= 40);
    auto mip=a;
    while (mip.width>1) mip=mip.downsampled();
    REQUIRE(mip.pixels.size() == 1);
    REQUIRE(mip.pixels[0].getAProperty() == 255);
}

TEST_CASE("needle detail is neutral deterministic opaque and mipmapped")
{
    const auto a=TrainSim::Render::makeNeedleDetailImage();
    const auto b=TrainSim::Render::makeNeedleDetailImage();
    REQUIRE(a.width==128);
    REQUIRE(a.pixels.size()==128*128);
    int minimum=255,maximum=0;
    for (std::size_t i=0;i<a.pixels.size();++i)
    {
        const auto& p=a.pixels[i];
        REQUIRE(p.getPackedValueProperty()==b.pixels[i].getPackedValueProperty());
        REQUIRE(p.getAProperty()==255);
        REQUIRE(p.getRProperty()==p.getGProperty());
        REQUIRE(p.getGProperty()==p.getBProperty());
        minimum=std::min(minimum,static_cast<int>(p.getRProperty()));
        maximum=std::max(maximum,static_cast<int>(p.getRProperty()));
    }
    REQUIRE(minimum>=180);
    REQUIRE(maximum-minimum>=35);
    auto mip=a;
    while (mip.width>1) mip=mip.downsampled();
    REQUIRE(mip.pixels.size()==1);
    REQUIRE(mip.pixels[0].getAProperty()==255);
}

TEST_CASE("urban ground uses bounded deterministic vegetation tint away from the railway")
{
    using TrainSim::Render::urbanGroundTint;
    for (double distance : {0.0,2300.0,125000.0,300000.0})
        for (double lateral : {-600.0,-100.0,-26.0,26.0,100.0,600.0})
        {
            const auto color = urbanGroundTint(distance,lateral);
            REQUIRE(color.getPackedValueProperty() == urbanGroundTint(distance,lateral).getPackedValueProperty());
            REQUIRE(color.getAProperty() == 255);
            REQUIRE(color.getGProperty() > color.getRProperty());
            REQUIRE(color.getGProperty() > color.getBProperty());
            REQUIRE(color.getGProperty() >= 95 && color.getGProperty() <= 125);
            const auto adjacent = urbanGroundTint(distance+0.01,lateral);
            REQUIRE(std::abs(static_cast<int>(color.getGProperty())-adjacent.getGProperty()) <= 1);
        }
}

TEST_CASE("terrain material image is deterministic opaque and tonally varied")
{
    const auto first = makeTerrainDetailImage(64);
    const auto second = makeTerrainDetailImage(64);
    REQUIRE(first.width == 64);
    REQUIRE(first.height == 64);
    REQUIRE(first.pixels.size() == 64U*64U);
    REQUIRE(first.pixels.size() == second.pixels.size());

    int minimum = 255;
    int maximum = 0;
    for (std::size_t index = 0; index < first.pixels.size(); ++index)
    {
        const auto& color = first.pixels[index];
        REQUIRE(color.getPackedValueProperty()
                == second.pixels[index].getPackedValueProperty());
        REQUIRE(color.getAProperty() == 255);
        REQUIRE(color.getRProperty() == color.getGProperty());
        REQUIRE(color.getGProperty() == color.getBProperty());
        const int red = static_cast<int>(color.getRProperty());
        minimum = std::min(minimum,red);
        maximum = std::max(maximum,red);
    }
    REQUIRE(minimum <= 205);
    REQUIRE(maximum >= 250);
    REQUIRE(maximum-minimum >= 40);
}

TEST_CASE("terrain material mip source downsamples to stable opaque levels")
{
    auto level = makeTerrainDetailImage(32);
    int levels = 1;
    while (level.width > 1 || level.height > 1)
    {
        level = level.downsampled();
        ++levels;
        REQUIRE(level.pixels.size()
                == static_cast<std::size_t>(level.width*level.height));
        for (const auto& color : level.pixels) REQUIRE(color.getAProperty() == 255);
    }
    REQUIRE(levels == 6);
    REQUIRE(level.width == 1);
    REQUIRE(level.height == 1);
}

TEST_CASE("ballast and carpet materials are deterministic neutral and distinct")
{
    const auto ballast = makeBallastDetailImage(64);
    const auto ballastAgain = makeBallastDetailImage(64);
    const auto carpet = makeCarpetDetailImage(64);
    REQUIRE(ballast.pixels.size() == 64U*64U);
    REQUIRE(carpet.pixels.size() == ballast.pixels.size());

    int ballastMinimum = 255;
    int ballastMaximum = 0;
    int carpetMinimum = 255;
    int carpetMaximum = 0;
    int differingTexels = 0;
    for (std::size_t index = 0; index < ballast.pixels.size(); ++index)
    {
        const auto& stone = ballast.pixels[index];
        const auto& repeatedStone = ballastAgain.pixels[index];
        const auto& fibre = carpet.pixels[index];
        REQUIRE(stone.getPackedValueProperty() == repeatedStone.getPackedValueProperty());
        REQUIRE(stone.getAProperty() == 255);
        REQUIRE(fibre.getAProperty() == 255);
        REQUIRE(stone.getRProperty() == stone.getGProperty());
        REQUIRE(stone.getGProperty() == stone.getBProperty());
        REQUIRE(fibre.getRProperty() == fibre.getGProperty());
        REQUIRE(fibre.getGProperty() == fibre.getBProperty());
        ballastMinimum = std::min(ballastMinimum,static_cast<int>(stone.getRProperty()));
        ballastMaximum = std::max(ballastMaximum,static_cast<int>(stone.getRProperty()));
        carpetMinimum = std::min(carpetMinimum,static_cast<int>(fibre.getRProperty()));
        carpetMaximum = std::max(carpetMaximum,static_cast<int>(fibre.getRProperty()));
        differingTexels += stone.getPackedValueProperty() != fibre.getPackedValueProperty();
    }
    REQUIRE(ballastMaximum-ballastMinimum >= 70);
    REQUIRE(carpetMaximum-carpetMinimum >= 40);
    REQUIRE(differingTexels > 3'500);
}

TEST_CASE("platform concrete keeps restrained aggregate contrast through mip generation")
{
    auto concrete = makeConcreteDetailImage(64);
    int minimum = 255;
    int maximum = 0;
    for (const auto& pixel : concrete.pixels)
    {
        minimum = std::min(minimum,static_cast<int>(pixel.getRProperty()));
        maximum = std::max(maximum,static_cast<int>(pixel.getRProperty()));
        REQUIRE(pixel.getRProperty() == pixel.getGProperty());
        REQUIRE(pixel.getGProperty() == pixel.getBProperty());
        REQUIRE(pixel.getAProperty() == 255);
    }
    REQUIRE(minimum >= 190);
    REQUIRE(maximum <= 250);
    REQUIRE(maximum-minimum >= 45);
    concrete = concrete.downsampled().downsampled();
    REQUIRE(concrete.width == 16);
    REQUIRE(concrete.height == 16);
}

TEST_CASE("interior panel material stays pale subtle and deterministic")
{
    const auto first = makeInteriorPanelDetailImage(64);
    const auto second = makeInteriorPanelDetailImage(64);
    int minimum = 255;
    int maximum = 0;
    for (std::size_t index = 0; index < first.pixels.size(); ++index)
    {
        const auto& pixel = first.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == second.pixels[index].getPackedValueProperty());
        REQUIRE(pixel.getAProperty() == 255);
        REQUIRE(pixel.getRProperty() == pixel.getGProperty());
        REQUIRE(pixel.getGProperty() == pixel.getBProperty());
        minimum = std::min(minimum,static_cast<int>(pixel.getRProperty()));
        maximum = std::max(maximum,static_cast<int>(pixel.getRProperty()));
    }
    REQUIRE(minimum >= 215);
    REQUIRE(maximum <= 250);
    REQUIRE(maximum-minimum >= 25);
}

TEST_CASE("water material has bounded directional variation")
{
    const auto water = makeWaterDetailImage(64);
    const auto repeated = makeWaterDetailImage(64);
    int minimum = 255;
    int maximum = 0;
    for (std::size_t index = 0; index < water.pixels.size(); ++index)
    {
        const auto& pixel = water.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == repeated.pixels[index].getPackedValueProperty());
        REQUIRE(pixel.getAProperty() == 255);
        minimum = std::min(minimum,static_cast<int>(pixel.getRProperty()));
        maximum = std::max(maximum,static_cast<int>(pixel.getRProperty()));
    }
    REQUIRE(minimum >= 205);
    REQUIRE(maximum <= 255);
    REQUIRE(maximum-minimum >= 30);
}

TEST_CASE("resilient transport floor has sparse bounded aggregate")
{
    const auto floor = makeResilientFloorDetailImage(64);
    const auto repeated = makeResilientFloorDetailImage(64);
    int minimum = 255;
    int maximum = 0;
    int darkChips = 0;
    for (std::size_t index = 0; index < floor.pixels.size(); ++index)
    {
        const auto& pixel = floor.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == repeated.pixels[index].getPackedValueProperty());
        REQUIRE(pixel.getAProperty() == 255);
        const int value = static_cast<int>(pixel.getRProperty());
        minimum = std::min(minimum,value);
        maximum = std::max(maximum,value);
        darkChips += value < 220;
    }
    REQUIRE(minimum >= 205);
    REQUIRE(maximum <= 250);
    REQUIRE(maximum-minimum >= 30);
    REQUIRE(darkChips > 25);
    REQUIRE(darkChips < 300);
}

TEST_CASE("masonry material retains alternating courses and mortar")
{
    const auto masonry = makeMasonryDetailImage(64);
    const auto repeated = makeMasonryDetailImage(64);
    int mortar = 0;
    int faces = 0;
    for (std::size_t index = 0; index < masonry.pixels.size(); ++index)
    {
        const auto& pixel = masonry.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == repeated.pixels[index].getPackedValueProperty());
        REQUIRE(pixel.getAProperty() == 255);
        const int value = static_cast<int>(pixel.getRProperty());
        REQUIRE(value >= 195);
        REQUIRE(value <= 246);
        mortar += value < 210;
        faces += value > 220;
    }
    REQUIRE(mortar > 500);
    REQUIRE(faces > 2'500);
    REQUIRE(masonry.pixels[0].getRProperty() < masonry.pixels[65].getRProperty());
}

TEST_CASE("rendered facade material has deterministic multiscale plaster variation")
{
    const auto first = makeRenderedFacadeDetailImage(64);
    const auto second = makeRenderedFacadeDetailImage(64);
    int minimum = 255;
    int maximum = 0;
    int shallowPits = 0;
    for (std::size_t index = 0; index < first.pixels.size(); ++index)
    {
        const auto& pixel = first.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == second.pixels[index].getPackedValueProperty());
        REQUIRE(pixel.getAProperty() == 255);
        REQUIRE(pixel.getRProperty() == pixel.getGProperty());
        REQUIRE(pixel.getGProperty() == pixel.getBProperty());
        const int value = static_cast<int>(pixel.getRProperty());
        minimum = std::min(minimum,value);
        maximum = std::max(maximum,value);
        shallowPits += value < 220;
    }
    REQUIRE(minimum >= 207);
    REQUIRE(maximum <= 250);
    REQUIRE(maximum-minimum >= 30);
    REQUIRE(shallowPits > 5);
    REQUIRE(shallowPits < 120);
}

TEST_CASE("cloud atlas preserves four distinct silhouettes and transparent tile borders")
{
    const auto atlas=makeCloudAtlasImage(64);
    REQUIRE(atlas.width==128);
    REQUIRE(atlas.height==128);
    const auto original=makeCloudDetailImage(64);
    for (unsigned variant=0;variant<4;++variant)
    {
        const auto tile=makeCloudDetailImage(64,variant);
        int changed=0;
        for (int y=0;y<64;++y)
            for (int x=0;x<64;++x)
            {
                const auto index=static_cast<std::size_t>(y*64+x);
                const auto target=static_cast<std::size_t>((static_cast<int>(variant/2)*64+y)*128
                    +static_cast<int>(variant%2)*64+x);
                REQUIRE(tile.pixels[index].getPackedValueProperty()==atlas.pixels[target].getPackedValueProperty());
                changed+=tile.pixels[index].getAProperty()!=original.pixels[index].getAProperty();
                if (x==0 || y==0 || x==63 || y==63)
                    REQUIRE(tile.pixels[index].getAProperty()<15);
            }
        if (variant!=0) REQUIRE(changed>1000);
    }
}

TEST_CASE("cloud material has a deterministic soft transparent silhouette")
{
    const auto cloud = makeCloudDetailImage(64);
    const auto repeated = makeCloudDetailImage(64);
    int transparent = 0;
    int feathered = 0;
    int dense = 0;
    int denseMinimum = 255;
    int denseMaximum = 0;
    for (std::size_t index = 0; index < cloud.pixels.size(); ++index)
    {
        const auto& pixel = cloud.pixels[index];
        REQUIRE(pixel.getPackedValueProperty()
                == repeated.pixels[index].getPackedValueProperty());
        const int alpha = static_cast<int>(pixel.getAProperty());
        transparent += alpha == 0;
        feathered += alpha > 0 && alpha < 210;
        dense += alpha >= 220;
        if (alpha >= 220)
        {
            denseMinimum=std::min(denseMinimum,static_cast<int>(pixel.getRProperty()));
            denseMaximum=std::max(denseMaximum,static_cast<int>(pixel.getRProperty()));
        }
        REQUIRE(pixel.getBProperty() >= pixel.getGProperty());
        REQUIRE(pixel.getGProperty() >= pixel.getRProperty());
    }
    REQUIRE(transparent > 1'000);
    REQUIRE(feathered > 300);
    REQUIRE(dense > 500);
    REQUIRE(denseMaximum-denseMinimum >= 25);
    REQUIRE(denseMinimum >= 170);
    REQUIRE(cloud.pixels.front().getAProperty() == 0);
    REQUIRE(cloud.pixels[32U*64U+32U].getAProperty() >= 220);
}

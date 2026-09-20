#include "original_road_contact.h"
#include "original_math.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace idas3::original {
namespace {
template<std::uint32_t Bits> constexpr float lit(){return std::bit_cast<float>(Bits);}
float bound(float value,float low,float high){
    if(low>value)return low;
    if(value>high)return high;
    return value;
}
OriginalContactPoint point(const OriginalDriveState& drive,std::size_t offset){
    return {drive.f(offset),drive.f(offset+4),drive.f(offset+8)};
}
void put(OriginalDriveState& drive,std::size_t offset,const OriginalContactPoint& value){
    for(std::size_t i=0;i<3;++i)drive.setf(offset+i*4,value[i]);
}
}

OriginalRoadContactServices bindOriginalRoadContactQueries(const OriginalCollisionData& data,
        OriginalTriangleSearchTrace& trace,OriginalSurfaceScratch& scratch,
        std::function<OriginalContactPoint(const OriginalActorState&,const OriginalContactPoint&)> transform){
    OriginalRoadContactServices result;
    result.transformPoint=std::move(transform);
    result.surface022CE0=[&data,&trace,&scratch](OriginalCollisionQuery& q){
        queryOriginalCollisionSurface(data,q,trace,scratch);
    };
    result.swept022D20=[&data,&trace,&scratch](OriginalCollisionQuery& q){
        queryOriginalCollisionSwept(data,q,trace,scratch);
    };
    return result;
}

OriginalRoadContactServices bindOriginalRoadContactServices(const OriginalCollisionData& data,
        OriginalTriangleSearchTrace& trace,OriginalSurfaceScratch& scratch,const OriginalFscaTable& fsca){
    return bindOriginalRoadContactQueries(data,trace,scratch,
        [&fsca](const OriginalActorState& actor,const OriginalContactPoint& p){
            const auto matrix=originalActorMatrix({actor.f(0),actor.f(4),actor.f(8)},
                {actor.f(24),actor.f(28),actor.f(32)},fsca);
            return transformOriginalPoint(matrix,p);
        });
}

OriginalContactPoint queryOriginalWheelSurface(OriginalDriveState& drive,
        OriginalRoadContactState& state,std::size_t corner,const OriginalContactPoint& current,
        const OriginalRoadContactServices& services){
    if(!services.surface022CE0)throw std::invalid_argument("Original surface query is required");
    auto& query=state.surfaces0CAA9518.at(corner);
    for(std::size_t i=0;i<3;++i)query.setu(44+4*i,query.u(32+4*i));
    for(std::size_t i=0;i<3;++i)query.setf(32+4*i,current[i]);
    services.surface022CE0(query);
    drive.setu(0x164+corner*4,query.u(28)&0xFFu);
    return {query.f(0)*1.0f,query.f(24)*1.0f,query.f(8)*1.0f};
}

void queryOriginalWheelSweep(OriginalDriveState& drive,OriginalRoadContactState& state,
        std::size_t corner,const OriginalContactPoint& current,const OriginalContactPoint& previous,
        const OriginalRoadContactServices& services){
    if(!services.swept022D20)throw std::invalid_argument("Original swept query is required");
    auto& query=state.sweeps0CAA9618.at(corner);
    for(std::size_t i=0;i<3;++i)query.setf(44+4*i,previous[i]);
    for(std::size_t i=0;i<3;++i)query.setf(32+4*i,current[i]);
    services.swept022D20(query);
    float magnitude=0.0f;
    // Exact 159B20 integer classifier: exponent-all-ones returns zero.
    if(((query.u(24)|0x807FFFFFu)+1u)!=0)magnitude=query.f(24);
    else ++state.invalidScalarDiagnostics;
    if((query.u(28)&0x8000u)==0 || !(query.f(24)>0.0f)){
        state.impacts0CAA9508[corner]=0.0f;
        return;
    }
    if(state.impacts0CAA9508[corner]==0.0f && drive.u(0x150)==0){
        state.feedback142460.push_back(2);
        drive.setu(0x3F4,drive.u(0x3F4)+1u);
        if(std::bit_cast<std::int32_t>(drive.u(0x3F4))<=99)
            state.impactRecords.push_back({point(drive,0),state.tick0C92DE30,magnitude});
        if((corner&1)==0)state.impact0C900E60=magnitude;
        else state.impact0C900E5C=magnitude;
        if(drive.u(0x148)!=0)drive.setu(0x3FC,drive.u(0x3FC)+1u);
    }
    drive.setu(0x150,1);drive.setu(0x154,std::uint32_t(corner));drive.setf(0x158,magnitude);
    state.normals0CAA94C8[corner]={query.f(0),query.f(4),query.f(8)};
    state.flags0CAA94F8[corner]=query.u(28);
    state.impacts0CAA9508[corner]=magnitude;
}

void aggregateOriginalWallImpacts(OriginalDriveState& drive,const OriginalRoadContactState& state){
    float x=state.normals0CAA94C8[1][0]*state.impacts0CAA9508[1];
    x=std::fma(state.normals0CAA94C8[0][0],state.impacts0CAA9508[0],x);
    x=std::fma(state.normals0CAA94C8[2][0],state.impacts0CAA9508[2],x);
    x=std::fma(state.normals0CAA94C8[3][0],state.impacts0CAA9508[3],x);
    float z=state.normals0CAA94C8[1][2]*state.impacts0CAA9508[1];
    z=std::fma(state.normals0CAA94C8[0][2],state.impacts0CAA9508[0],z);
    z=std::fma(state.normals0CAA94C8[2][2],state.impacts0CAA9508[2],z);
    z=std::fma(state.normals0CAA94C8[3][2],state.impacts0CAA9508[3],z);
    if(!(x==0.0f&&z==0.0f)){
        drive.setf(0x258,bound(x*1.0f,-10.0f,10.0f));
        drive.setf(0x25C,bound(z*1.0f,-10.0f,10.0f));
    }else{
        drive.setf(0x258,drive.f(0x258)*lit<0x3EE147AE>());
        drive.setf(0x25C,drive.f(0x25C)*lit<0x3EE147AE>());
    }
}

void updateOriginalRoadContact(OriginalDriveState& drive,OriginalActorState& actor,
        OriginalRoadContactState& state,const OriginalRoadContactParameters& parameters,
        const OriginalRoadContactServices& services){
    if(!services.transformPoint||!services.surface022CE0||!services.swept022D20)
        throw std::invalid_argument("Original matrix, surface and swept query services are required");
    std::array<OriginalContactPoint,4> oldCorners{},corners{},wallCorners{};
    for(std::size_t i=0;i<4;++i)oldCorners[i]=point(drive,680+i*12);
    float probeDepth=1.0f;
    for(std::size_t i=0;i<4;++i)if(drive.u(356+i*4)==28){
        drive.setu(316,drive.u(316)+1u);break;
    }
    if(drive.u(316)!=0){probeDepth=lit<0x3E0F5C29>();drive.setu(316,drive.u(316)+1u);}
    if(std::bit_cast<std::int32_t>(drive.u(316))>55){
        drive.setu(316,0);drive.setf(936,lit<0x3E4CCCCD>());
        state.feedback142460.push_back(6);drive.setu(1028,drive.u(1028)+1u);
    }
    if(drive.f(936)>0.0f)drive.setf(936,drive.f(936)-lit<0x3C23D70A>());
    if(0.0f>drive.f(936))drive.setf(936,0.0f);
    const float oscillation=originalSinF32(drive.f(944));
    float amplitude=drive.f(936);
    if(!(lit<0x3E4CCCCD>()>amplitude))amplitude=lit<0x3E4CCCCD>();
    drive.setf(940,amplitude*oscillation);
    float phase=drive.f(944)+lit<0x3F333333>();
    if(std::abs(phase)>lit<0x40490FDB>()){
        if(0.0f>-phase)phase=lit<0xC0C90FDB>()+phase;
        else phase=lit<0x40C90FDB>()+phase;
    }
    drive.setf(944,phase);
    const auto& t=parameters.geometry0C2700F4;
    const std::array<OriginalContactPoint,4> local={{{t[0],-probeDepth,t[2]},
        {-t[0],-probeDepth,t[2]},{t[1],-probeDepth,-t[3]},
        {-t[1],-probeDepth,-t[3]}}};
    for(std::size_t i=0;i<4;++i)corners[i]=services.transformPoint(actor,local[i]);
    const float frontHeight=-t[8]-t[10],rearHeight=-t[9]-t[10];
    const std::array<OriginalContactPoint,4> wallLocal={{{t[4],frontHeight,t[6]},
        {-t[4],frontHeight,t[6]},{t[5],rearHeight,-t[7]},
        {-t[5],rearHeight,-t[7]}}};
    for(std::size_t i=0;i<4;++i)wallCorners[i]=services.transformPoint(actor,wallLocal[i]);
    for(std::size_t i=0;i<4;++i)put(drive,680+i*12,corners[i]);
    for(std::size_t i=0;i<4;++i){
        OriginalContactPoint displacement{};
        for(std::size_t j=0;j<3;++j)displacement[j]=oldCorners[i][j]-corners[i][j];
        if(displacement[0]==0.0f&&displacement[1]==0.0f&&displacement[2]==0.0f)
            displacement=point(drive,776+i*12);
        put(drive,728+i*12,displacement);
    }
    drive.setu(336,0);state.impact0C900E60=0.0f;state.impact0C900E5C=0.0f;
    const OriginalContactPoint actorPosition={actor.f(0),actor.f(4),actor.f(8)};
    for(std::size_t i=0;i<4;++i)
        queryOriginalWheelSweep(drive,state,i,wallCorners[i],actorPosition,services);
    for(std::size_t i=0;i<4;++i)drive.setf(948+i*4,0.0f);
    std::array<OriginalContactPoint,4> surface{};
    for(std::size_t i=0;i<4;++i)surface[i]=queryOriginalWheelSurface(drive,state,i,corners[i],services);
    // The original uses a negative scalar for lift. Below zero it first
    // substitutes -0; less than -4 overrides that with -1, in this order.
    for(std::size_t i=0;i<4;++i){
        const float correction=surface[i][1]*-1.0f;
        float applied=correction;
        drive.setu(300+i*4,0);
        if(0.0f>correction){applied=-0.0f;drive.setu(300+i*4,1);}
        if(-4.0f>correction)applied=-1.0f;
        corners[i][1]+=applied;
    }
    for(std::size_t i=0;i<4;++i){
        const float difference=corners[i][1]-drive.f(196+i*4);
        drive.setf(948+i*4,bound(-difference*lit<0x3727C5AC>(),
            lit<0xBD8F5C29>(),lit<0x3D8F5C29>()));
        drive.setf(196+i*4,corners[i][1]);
    }
    for(std::size_t i=0;i<4;++i){
        OriginalContactPoint relative{};
        for(std::size_t j=0;j<3;++j)relative[j]=corners[i][j]-actorPosition[j];
        put(drive,72+i*12,relative);
    }
    OriginalContactPoint delta{};
    for(std::size_t j=0;j<3;++j)delta[j]=drive.f(48+j*4)-drive.f(36+j*4);
    put(drive,60,delta);
    std::array<float,4> target{};
    for(std::size_t i=0;i<4;++i){
        float value=originalFiprDot3(delta,point(drive,72+i*12));value*=20.0f;
        target[i]=bound(value,i<2?lit<0xBCA3D70A>():lit<0xBD75C28F>(),lit<0x3D75C28F>());
    }
    for(std::size_t i=0;i<4;++i){
        float change=target[i]-drive.f(148+i*4);change*=lit<0x3C23D70A>();
        drive.setf(180+i*4,change);
        drive.setf(164+i*4,drive.f(164+i*4)+change);
    }
    for(std::size_t i=0;i<4;++i)
        drive.setf(164+i*4,bound(drive.f(164+i*4),lit<0xBC03126F>(),lit<0x3C03126F>()));
    for(std::size_t i=0;i<4;++i){
        drive.setf(148+i*4,drive.f(148+i*4)+drive.f(164+i*4));
        drive.setf(164+i*4,drive.f(164+i*4)*0.75f);
    }
    // Rear forces are added first in the source; fields are independent.
    for(std::size_t i=2;i<4;++i)drive.setf(948+i*4,drive.f(948+i*4)+drive.f(148+i*4));
    for(std::size_t i=0;i<2;++i){
        float force=drive.f(948+i*4)+drive.f(148+i*4);force+=drive.f(940);
        drive.setf(948+i*4,force);
    }
    for(std::size_t i=0;i<4;++i)
        drive.setf(948+i*4,bound(drive.f(948+i*4),lit<0xBDD70A3E>(),lit<0x3DD70A3E>()));
    for(std::size_t i=0;i<4;++i)if(drive.u(300+i*4)!=0)drive.setf(948+i*4,lit<0x3D8F5C29>());
    for(std::size_t i=0;i<4;++i){
        actor.setf(64+i*4,drive.f(948+i*4));
        drive.setf(964+i*4,drive.f(948+i*4)/lit<0x3D8F5C29>());
    }
    // Preserve the original sum grouping; algebraic regrouping loses bits.
    float rear=corners[2][1]+actor.f(72);rear+=corners[3][1];rear+=actor.f(76);
    float front=corners[0][1]+actor.f(64);front+=corners[1][1];front+=actor.f(68);
    float pitchDifference=rear-front;pitchDifference*=0.5f;
    float right=corners[0][1]+actor.f(64);right+=corners[2][1];right+=actor.f(72);
    float left=corners[1][1]+actor.f(68);left+=corners[3][1];left+=actor.f(76);
    float rollDifference=right-left;rollDifference*=0.5f;
    float height=corners[0][1]+actor.f(64);height+=corners[1][1];height+=actor.f(68);
    height+=corners[2][1];height+=actor.f(72);height+=corners[3][1];height+=actor.f(76);height*=0.25f;
    float pitch=float(std::bit_cast<std::int32_t>(originalAsinAngle(pitchDifference/(t[2]+t[3]))));
    pitch*=lit<0x40490FDB>();pitch*=lit<0x38000000>();
    float roll=float(std::bit_cast<std::int32_t>(originalAsinAngle(rollDifference/(t[0]+t[1]))));
    roll*=lit<0x40490FDB>();roll*=lit<0x38000000>();
    if(pitch>lit<0x3E8F5C29>()&&lit<0x4048F5C3>()>pitch)pitch=lit<0x3E8F5C29>();
    if(pitch>lit<0x4048F5C3>()&&6.0f>pitch)pitch=6.0f;
    if(roll>lit<0x3D8F5C29>()&&lit<0x4048F5C3>()>roll)roll=lit<0x3D8F5C29>();
    if(roll>lit<0x4048F5C3>()&&lit<0x40C6B852>()>roll)roll=lit<0x40C6B852>();
    drive.setf(12,pitch);drive.setf(20,roll);drive.setf(4,height);
    for(std::size_t i=0;i<3;++i){actor.setu(i*4,drive.u(i*4));actor.setu(24+i*4,drive.u(12+i*4));}
    aggregateOriginalWallImpacts(drive,state);
}
} // namespace idas3::original

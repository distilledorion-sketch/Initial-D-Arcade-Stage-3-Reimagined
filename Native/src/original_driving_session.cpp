#include "original_driving_session.h"
#include "original_math.h"
#include "original_session_initialization.h"
#include "original_rival_setup.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace idas3::original {
void initializeOriginalRecoveryBackup(const OriginalDriveState& drive,OriginalRecoveryState& recovery){
    std::copy_n(drive.words.begin(),272,recovery.drive0C9009F0.words.begin());
    for(const auto offset:{356u,360u,364u,368u})recovery.drive0C9009F0.setu(offset,0);
}
struct DrivingMutableState {
    OriginalDrivingSelection selection;
    OriginalVehicleState vehicle;
    OriginalVehicleParameters parameters;
    OriginalActorState actor;
    OriginalActorState secondaryActor;
    std::array<OriginalRivalState,8> rivalActors;
    OriginalRivalRoadState rivalRoad;
    OriginalRivalPaceInputs rivalInputs;
    OriginalRivalInitializationResult rivalInitialization;
    OriginalBodyContactState bodyContact;
    std::uint32_t rivalFrame=0,rivalGeometry=0;
    bool rivalInitialized=false;
    OriginalPublishedActors published;
    OriginalRecoveryState recovery;
    OriginalWheelHistory wheels;
    OriginalRoadContactState road;
    OriginalRoadContactParameters roadParameters;
    OriginalTriangleSearchTrace trace;
    OriginalSurfaceScratch scratch;
    OriginalContactCompletionState completion;
    OriginalInitializationSideState initialization;
    OriginalSessionInitializationState boot;
    std::uint32_t frame=0;
    std::uint8_t digitalByte=0;
    std::uint8_t automaticBrake0C2F4DE0=0;
};

struct OriginalDrivingSession::Impl : DrivingMutableState {
    OriginalPhysicsData data;
    OriginalPhysicsPath path;
    OriginalFscaTable fsca;
    OriginalCollisionData collision;
    OriginalRivalData rivalData;
    OriginalRivalPath rivalPath;
    OriginalRoadContactServices services;
    OriginalContactEngineOutput engineOutput;
    std::shared_ptr<const int> epoch=std::make_shared<const int>(0);
};
struct OriginalDrivingSession::Checkpoint::Data {
    DrivingMutableState state;
    std::shared_ptr<const int> epoch;
};
std::size_t OriginalDrivingSession::Checkpoint::memoryBytes() const {
    if(!data_)return 0;
    return sizeof(Data)+data_->state.road.impactRecords.capacity()*sizeof(OriginalImpactRecord)
        +data_->state.road.feedback142460.capacity()*sizeof(std::uint32_t);
}
bool OriginalDrivingSession::rollbackSafe() const {return impl_&&!impl_->engineOutput;}
OriginalDrivingSession::Checkpoint OriginalDrivingSession::checkpoint() const {
    if(!rollbackSafe())throw std::logic_error("Rollback requires a numerical session without external engine/audio callbacks");
    Checkpoint out;
    out.data_=std::make_shared<Checkpoint::Data>(Checkpoint::Data{state(),state().epoch});
    return out;
}
void OriginalDrivingSession::restore(const Checkpoint& saved) {
    if(!rollbackSafe()||!saved.data_||saved.data_->epoch!=state().epoch)
        throw std::invalid_argument("Rollback checkpoint belongs to another session/reset or has external side effects");
    // Keep the immutable asset owner and its bound collision services alive.
    static_cast<DrivingMutableState&>(state())=saved.data_->state;
}
std::uint64_t OriginalDrivingSession::rollbackDigest() const {
    const auto& s=state();std::uint64_t hash=14695981039346656037ull;
    auto add=[&](const auto& value){
        // These records contain only 32-bit words/scalars/arrays; no pointers,
        // bools, vectors or owner identity enter the diagnostic digest.
        const auto bytes=std::as_bytes(std::span(&value,1));
        for(auto byte:bytes){hash^=std::to_integer<unsigned char>(byte);hash*=1099511628211ull;}
    };
    add(s.vehicle.drive.words);add(s.vehicle.loss);add(s.vehicle.tail);
    add(s.vehicle.transmission);add(s.vehicle.transmissionGlobals);add(s.vehicle.controls);
    add(s.actor.words);add(s.secondaryActor.words);add(s.published);add(s.recovery);add(s.wheels);
    add(s.road.surfaces0CAA9518);add(s.road.sweeps0CAA9618);add(s.road.normals0CAA94C8);
    add(s.road.flags0CAA94F8);add(s.road.impacts0CAA9508);add(s.road.impact0C900E5C);add(s.road.impact0C900E60);
    add(s.road.tick0C92DE30);add(s.trace);add(s.scratch);add(s.completion);
    add(s.rivalActors);add(s.rivalRoad);add(s.rivalFrame);add(s.bodyContact);
    add(s.initialization);add(s.boot);add(s.frame);
    hash^=s.digitalByte;hash*=1099511628211ull;hash^=s.automaticBrake0C2F4DE0;hash*=1099511628211ull;
    return hash;
}

OriginalDrivingSession::OriginalDrivingSession()=default;
OriginalDrivingSession::~OriginalDrivingSession()=default;
OriginalDrivingSession::OriginalDrivingSession(OriginalDrivingSession&&) noexcept=default;
OriginalDrivingSession& OriginalDrivingSession::operator=(OriginalDrivingSession&&) noexcept=default;
OriginalDrivingSession::Impl& OriginalDrivingSession::state(){
    if(!impl_)throw std::logic_error("Original driving session has not been reset");return *impl_;
}
const OriginalDrivingSession::Impl& OriginalDrivingSession::state() const{
    if(!impl_)throw std::logic_error("Original driving session has not been reset");return *impl_;
}
bool OriginalDrivingSession::ready() const noexcept{return bool(impl_);}

OriginalInitializationResult OriginalDrivingSession::reset(const std::filesystem::path& root,const OriginalDrivingSelection& selection,
        const std::array<float,3>& position,const std::array<float,3>& angles,const ImportedDrivingRoad* importedRoad){
    if(selection.physics.conditionCode>=18||selection.collisionVariant>1)
        throw std::invalid_argument("Original session requires condition0..17 and explicit collision selector0/1");
    if(selection.collisionVariant!=(selection.physics.conditionCode&1u))
        throw std::invalid_argument("Original standard course setup requires collision selector equal to condition low bit");
    for(std::size_t i=0;i<3;++i)if(!std::isfinite(position[i])||!std::isfinite(angles[i]))
        throw std::invalid_argument("Original reset pose must be finite");
    if(selection.rival){
        const auto& rival=*selection.rival;
        if(rival.enemyId0C9015E0>=32||rival.geometryCar0C9015F8>=35)
            throw std::invalid_argument("Original rival profile/geometry outside recovered tables");
        if(rival.control>=0)for(unsigned i=0;i<3;++i)if(!std::isfinite(rival.position[i])||!std::isfinite(rival.angles[i]))
            throw std::invalid_argument("Original rival reset pose must be finite");
    }
    auto next=std::make_unique<Impl>();
    // Canonical program's C rand state at0C37C778 is initialized to1.
    next->initialization.randomSeed0C37C778=1;
    // Preserve the original initializer's unwritten state on a subsequent
    // reset. New host sessions begin with zero-initialized static storage.
    if(impl_){
        next->vehicle=impl_->vehicle;next->actor=impl_->actor;next->wheels=impl_->wheels;
        next->secondaryActor=impl_->secondaryActor;next->published=impl_->published;next->recovery=impl_->recovery;
        next->road=impl_->road;next->trace=impl_->trace;next->scratch=impl_->scratch;
        next->completion=impl_->completion;next->initialization=impl_->initialization;
        next->boot=impl_->boot;next->frame=impl_->frame;next->digitalByte=impl_->digitalByte;
        next->rivalActors=impl_->rivalActors;next->rivalRoad=impl_->rivalRoad;next->rivalInputs=impl_->rivalInputs;
        next->rivalInitialization=impl_->rivalInitialization;next->rivalFrame=impl_->rivalFrame;
        next->rivalGeometry=impl_->rivalGeometry;next->rivalPath=impl_->rivalPath;next->bodyContact=impl_->bodyContact;
    }
    next->selection=selection;
    const auto control=selection.rival?selection.rival->control:selection.physics.progressEnabled0C9015E4?-2:-1;
    next->selection.physics.progressEnabled0C9015E4=control==-2?1u:0u;
    next->rivalInputs.aiDifficulty=selection.rival?std::min(selection.rival->aiDifficulty,2u):0u;
    if(selection.rival){
        next->selection.physics.vehicleMode0C9015E0=selection.rival->enemyId0C9015E0;
        next->rivalInputs.level0C9015D0=selection.rival->level0C9015D0;
        next->rivalInputs.opponentProgress0C901644=selection.rival->opponentProgress0C901644;
        next->rivalInputs.progress0C901604=selection.rival->progress0C901604;
        next->rivalGeometry=selection.rival->geometryCar0C9015F8;
    }
    next->rivalInputs.condition0C9015CC=selection.physics.conditionCode;
    const auto& effective=next->selection.physics;
    const auto dataRoot=root/"data"/"original_physics";
    next->data=OriginalPhysicsData::load(dataRoot/"tables.bin");
    next->path=next->data.loadPath(dataRoot,selection.physics.conditionCode);
    next->fsca=OriginalFscaTable::load(dataRoot/"fsca_table.bin");
    next->rivalData=OriginalRivalData::load(root/"data/original_rival");
    next->collision=OriginalCollisionData::load(dataRoot/originalCollisionFile(selection.physics.conditionCode,selection.collisionVariant));
    next->parameters=next->data.parameters(effective,next->path);
    if(importedRoad){
        if(selection.rival||importedRoad->path.points.size()<21||importedRoad->path.points.size()>16000||
            importedRoad->path.inclusiveLastIndex+1!=importedRoad->path.points.size()||importedRoad->collision.triangles.empty())
            throw std::invalid_argument("Invalid imported solo road");
        next->path=importedRoad->path;next->path.conditionCode=effective.conditionCode;
        next->collision=importedRoad->collision;
        next->parameters.road={effective.conditionCode,0,next->path.points,next->path.inclusiveLastIndex};
    }
    // A solo/online car may retain its last Legend opponent in the save.
    // Road oil exclusions belong to this race's opponent, not that history.
    // Keep tuning/initialization selection intact; only clear the road gate
    // when no rival setup exists (Bunta retains its explicit rival setup).
    if(!selection.rival)next->parameters.road.mode0C9015E0=0;
    const auto geometry=next->data.carRecord(selection.physics.vehicleIndex);
    for(std::size_t i=0;i<11;++i)next->roadParameters.geometry0C2700F4[i]=std::bit_cast<float>(geometry[i]);
    const auto inputs=next->data.initialization(effective,position,angles);
    const auto effects=initializeOriginalSession(next->vehicle,next->parameters,next->initialization,next->actor,
        next->road,next->boot,inputs,verifiedOriginalSessionDefaults());
    if(importedRoad){
        float best=std::numeric_limits<float>::max();unsigned nearest=0;
        for(unsigned i=0;i<next->path.points.size();i++){
            const auto& p=next->path.points[i];const float dx=p[0]-position[0],dz=p[2]-position[2],d=dx*dx+dz*dz;
            if(d<best){best=d;nearest=i;}
        }
        next->vehicle.drive.setu(0x118,nearest);
    }
    //159720: player initialization precedes sparse rival reset and optional
    //15AE00. The player's initial enemy-mask lookup therefore uses the
    // requested profile, before15AE00 can override shared enemyID to31.
    disableOriginalRivals(next->rivalActors,next->rivalFrame);
    if(control>=0){
        const auto& requested=*selection.rival;
        OriginalRivalInitializationInputs rivalIn{requested.position,requested.angles,selection.physics.conditionCode,
            requested.enemyId0C9015E0,requested.profileMode0C901648,requested.level0C9015D0,1,1};
        next->rivalInitialization=initializeOriginalRival(next->rivalActors[1],next->secondaryActor,next->rivalRoad.surfaces0CAA9764,rivalIn);
        const auto& initialized=next->rivalInitialization;
        next->rivalInitialized=true;next->rivalFrame=initialized.frame0CAA986C;
        next->rivalInputs.profile0CAA9868=initialized.profile0CAA9868;
        next->rivalInputs.level0C9015D0=initialized.level0C9015D0;
        next->selection.physics.vehicleMode0C9015E0=initialized.enemyId0C9015E0;
        next->parameters.road.mode0C9015E0=initialized.enemyId0C9015E0;
        next->rivalPath=next->rivalData.loadPath(root/"data/original_rival",selection.physics.conditionCode,initialized.alternatePath);
        //901728 is shared: profile26's alternate must also become the
        // player's nearest-path search. Both original bounds tables agree.
        if(next->path.inclusiveLastIndex!=next->rivalPath.inclusiveLastIndex)
            throw std::runtime_error("Original player/rival shared path bounds disagree");
        next->path.points.assign(next->rivalPath.points.begin(),next->rivalPath.points.begin()+next->path.inclusiveLastIndex+1);
        next->parameters.road.path0C901728=next->path.points;
    }
    publishOriginalActors(next->actor,next->secondaryActor,next->selection.physics.progressEnabled0C9015E4,next->published);
    initializeOriginalRecoveryBackup(next->vehicle.drive,next->recovery);
    next->completion.elapsedFrames0C900E84=std::bit_cast<std::uint32_t>(next->boot.elapsedFrames0C900E84);
    next->completion.steeringMask0C900EBC=next->boot.steeringMask0C900EBC;
    next->completion.previousFlag0CAA94C4=next->boot.previousFlag0CAA94C4;
    next->completion.randomSeed0C37C778=next->initialization.randomSeed0C37C778;
    //1595C0 restores the original append pointer words but not record contents.
    next->completion.positionCursor=0;next->completion.frameCursor=0;
    next->services=bindOriginalRoadContactServices(next->collision,next->trace,next->scratch,next->fsca);
    // Outer062FA0 clears its race auto-brake and start requests after setup.
    // Preserve actor bit14; the next157AE0 republishes the freshly initialized
    // drive+1AC stopped latch into that bit.
    next->actor.setu(0x50,next->actor.u(0x50)&~0xA000u);
    impl_=std::move(next);
    return effects;
}

OriginalDrivingStepEffects OriginalDrivingSession::tick(const OriginalVehicleInputs& incoming,
        const OriginalBodyCollisionResult* sharedContact,const OriginalContactEngineOutput& frameEngineOutput){
    auto& s=state();OriginalDrivingStepEffects out;out.frame0C92DE30=s.frame;
    auto inputs=incoming;
    inputs.elapsedFrames0C900E84=std::bit_cast<std::int32_t>(s.completion.elapsedFrames0C900E84);
    s.parameters.steeringMemory.mask0C900EBC=s.completion.steeringMask0C900EBC;
    s.actor.setu(0x50,(s.actor.u(0x50)&~0x8000u)|(inputs.gearEnabled?0x8000u:0u));
    //159920 first runs157880 against the previous publications, including
    // their preserved secondary pose after a negative-control reset.
    // A synchronized two-human owner can supply both halves of one contact
    // solve using the same prior-frame poses. Ordinary solo/AI keeps its path.
    out.bodyCollision=sharedContact?*sharedContact:s.selection.bodyContactEnabled
        ?produceOriginalBodyContact(s.published,s.bodyContact,s.rivalData,s.fsca):OriginalBodyCollisionResult{};
    const auto bodyEffects=applyOriginalBodyCollisionResponse(s.vehicle.drive,out.bodyCollision,s.boot.state0C31FD44,s.completion.randomSeed0C37C778);
    out.vehicle=stepOriginalVehicle(s.vehicle,inputs,s.parameters,{originalSinF32,originalCosF32,originalFiprDot3});
    prepareOriginalContactFrame(s.vehicle.drive,s.actor,s.wheels);
    s.road.tick0C92DE30=s.frame;
    const auto oldImpactCount=s.road.impactRecords.size();
    updateOriginalRoadContact(s.vehicle.drive,s.actor,s.road,s.roadParameters,s.services);
    const auto addedImpacts=std::uint32_t(s.road.impactRecords.size()-oldImpactCount);
    auto& stats=s.vehicle.tail.statistics0C91FB0C;
    stats[5]+=addedImpacts*12u;stats[6]+=addedImpacts*4u;stats[7]+=addedImpacts*4u;
    publishOriginalActors(s.actor,s.secondaryActor,s.selection.physics.progressEnabled0C9015E4,s.published);
    s.completion.cues0C900E5C[0]=std::bit_cast<std::uint32_t>(s.road.impact0C900E5C);
    s.completion.cues0C900E5C[1]=std::bit_cast<std::uint32_t>(s.road.impact0C900E60);
    const auto oldPositionCursor=s.completion.positionCursor,oldFrameCursor=s.completion.frameCursor;
    out.completion=finishOriginalContactFrame(s.vehicle.drive,s.vehicle.transmission,s.completion,{s.frame,s.digitalByte},frameEngineOutput?frameEngineOutput:s.engineOutput);
    stats[9]+=(s.completion.positionCursor-oldPositionCursor)*12u;
    stats[10]+=(s.completion.frameCursor-oldFrameCursor)*4u;
    //159920 runs15B0A0(1,1) after player completion. Its four private
    // queries share the locator trace/surface scratch with player queries.
    s.rivalRoad.trace=s.trace;s.rivalRoad.surface=s.scratch;
    out.rivalAdvanced=updateOriginalRival(s.rivalActors[1],s.secondaryActor,s.rivalFrame,s.rivalData,s.rivalPath,
        s.rivalInputs,s.vehicle.drive,s.actor,s.rivalGeometry,s.rivalRoad,s.collision,s.fsca);
    s.trace=s.rivalRoad.trace;s.scratch=s.rivalRoad.surface;
    // Publish a second time before recovering only the player's state.
    publishOriginalActors(s.actor,s.secondaryActor,s.selection.physics.progressEnabled0C9015E4,s.published);
    out.restoredValidRoadState=applyOriginalRecovery(s.vehicle.drive,s.actor,s.published,s.recovery);
    s.initialization.randomSeed0C37C778=s.completion.randomSeed0C37C778;
    s.boot.elapsedFrames0C900E84=std::bit_cast<std::int32_t>(s.completion.elapsedFrames0C900E84);
    s.boot.steeringMask0C900EBC=s.completion.steeringMask0C900EBC;
    s.boot.previousFlag0CAA94C4=s.completion.previousFlag0CAA94C4;
    out.feedback142460=std::move(s.road.feedback142460);s.road.feedback142460.clear();
    if(bodyEffects.cue)out.feedback142460.insert(out.feedback142460.begin(),bodyEffects.cue);
    out.newImpactRecords=std::move(s.road.impactRecords);s.road.impactRecords.clear();
    out.invalidScalarDiagnostics=std::exchange(s.road.invalidScalarDiagnostics,0)+bodyEffects.invalidScalarDiagnostics;
    ++s.frame;
    return out;
}
void OriginalDrivingSession::setPlatformFrame(std::uint32_t frame,std::uint8_t digital){auto& s=state();s.frame=frame;s.digitalByte=digital;}
void OriginalDrivingSession::setEngineOutput(OriginalContactEngineOutput output){state().engineOutput=std::move(output);}
void OriginalDrivingSession::finishSoundFrame(const std::function<void(std::uint32_t&)>& output){withSharedRandom(output);}
void OriginalDrivingSession::withSharedRandom(const std::function<void(std::uint32_t&)>& action){auto& s=state();if(action)action(s.completion.randomSeed0C37C778);s.initialization.randomSeed0C37C778=s.completion.randomSeed0C37C778;}
void OriginalDrivingSession::setProgressCorrection(float progress,std::uint32_t mode){
    if(!std::isfinite(progress))throw std::invalid_argument("Original frame progress must be finite");
    auto& s=state();s.parameters.frame.global0C901650=progress;s.parameters.frame.global0C9015D4=mode;
    s.selection.physics.progress0C901650=progress;s.selection.physics.progressMode0C9015D4=mode;
}
void OriginalDrivingSession::setRaceAutomaticBrake(bool enabled){
    auto& s=state();
    s.actor.setu(0x50,(s.actor.u(0x50)&~0x2000u)|(enabled?0x2000u:0u));
    s.automaticBrake0C2F4DE0=enabled?1u:0u;
}
void OriginalDrivingSession::enableRaceStart(std::uint32_t mode){
    if(mode!=0&&mode!=2)throw std::invalid_argument("Original local race start supports numeric modes0/2");
    auto& s=state();s.actor.setu(0x50,s.actor.u(0x50)|0x8000u);
    if(mode==0)s.secondaryActor.setu(0x50,s.secondaryActor.u(0x50)|0x8000u);
}
std::uint8_t OriginalDrivingSession::raceAutomaticBrakeByte() const{return state().automaticBrake0C2F4DE0;}
bool OriginalDrivingSession::stoppedForRace() const{return (state().actor.u(0x50)&0x4000u)!=0;}
std::uint32_t OriginalDrivingSession::platformFrame() const{return state().frame;}
const OriginalDrivingSelection& OriginalDrivingSession::selection() const{return state().selection;}
const OriginalVehicleState& OriginalDrivingSession::vehicle() const{return state().vehicle;}
const OriginalActorState& OriginalDrivingSession::actor() const{return state().actor;}
const OriginalVehicleParameters& OriginalDrivingSession::parameters() const{return state().parameters;}
const OriginalPhysicsPath& OriginalDrivingSession::path() const{return state().path;}
const OriginalCollisionData& OriginalDrivingSession::collision() const{return state().collision;}
const OriginalRoadContactState& OriginalDrivingSession::roadContact() const{return state().road;}
const OriginalContactCompletionState& OriginalDrivingSession::contactCompletion() const{return state().completion;}
const OriginalWheelHistory& OriginalDrivingSession::wheelHistory() const{return state().wheels;}
const OriginalInitializationSideState& OriginalDrivingSession::initializationSide() const{return state().initialization;}
const OriginalPublishedActors& OriginalDrivingSession::publishedActors() const{return state().published;}
const OriginalRecoveryState& OriginalDrivingSession::recovery() const{return state().recovery;}
bool OriginalDrivingSession::rivalInitialized() const{return state().rivalInitialized;}
bool OriginalDrivingSession::rivalActive() const{return state().rivalActors[1].u(0)!=0;}
const OriginalRivalState& OriginalDrivingSession::rivalActor() const{return state().rivalActors[1];}
const OriginalActorState& OriginalDrivingSession::rivalPublicActor() const{return state().secondaryActor;}
const OriginalRivalInitializationResult& OriginalDrivingSession::rivalInitialization() const{return state().rivalInitialization;}
const OriginalRivalPaceInputs& OriginalDrivingSession::rivalPaceInputs() const{return state().rivalInputs;}
const OriginalRivalPath& OriginalDrivingSession::rivalPath() const{return state().rivalPath;}
const OriginalRivalRoadState& OriginalDrivingSession::rivalRoadContact() const{return state().rivalRoad;}
std::uint32_t OriginalDrivingSession::rivalFrameCounter() const{return state().rivalFrame;}
const OriginalBodyContactState& OriginalDrivingSession::bodyContact() const{return state().bodyContact;}
} // namespace idas3::original

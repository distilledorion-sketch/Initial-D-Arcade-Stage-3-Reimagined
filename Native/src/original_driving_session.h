#pragma once
#include "original_contact_completion.h"
#include "original_data.h"
#include "original_frame_state.h"
#include "original_road_contact.h"
#include "original_body_contact.h"
#include "original_rival_initialization.h"
#include "original_rival_motion.h"
#include <memory>
#include <optional>

namespace idas3::original {
// Track data adapter; stepping remains in the existing original vehicle owner.
struct ImportedDrivingRoad { OriginalPhysicsPath path; OriginalCollisionData collision; };

// Outer race initializer159870..159888: copy exactly272 drive words and
// invalidate four saved wheel materials. Preserve the saved actor and suffix.
void initializeOriginalRecoveryBackup(const OriginalDriveState& drive,OriginalRecoveryState& recovery);
struct OriginalDrivingRivalSetup {
    std::uint32_t aiDifficulty=0; // Captured once for the next offline battle.
    //159720 stack+12; distinct from numeric race mode and profile mode.
    std::int32_t control=-1;
    std::uint32_t profileMode0C901648=0,enemyId0C9015E0=0,level0C9015D0=0;
    //159720 profile+20, not inferred from the public car-ID lookup.
    std::uint32_t geometryCar0C9015F8=0;
    std::array<float,3> position{},angles{};
    std::uint32_t opponentProgress0C901644=0;
    std::array<std::uint32_t,8> progress0C901604{};
};
struct OriginalDrivingSelection {
    OriginalPhysicsSelection physics;
    // Original042700 r6 selector; independent of the path direction code.
    // Caller must supply the recovered course-setup value explicitly.
    std::uint32_t collisionVariant=0;
    // Absent keeps the solo caller contract. The complete original inactive
    // rival/pair stages still execute. Explicit setup owns its source fields.
    std::optional<OriginalDrivingRivalSetup> rival;
    // The host disables the retained secondary actor for a solo race.
    // Source-parity callers keep the original pair stage by default.
    // An explicit synchronized contact result always takes precedence.
    bool bodyContactEnabled=true;
};
struct OriginalDrivingStepEffects {
    OriginalVehicleStepResult vehicle;
    OriginalContactCompletionEffects completion;
    std::vector<std::uint32_t> feedback142460;
    std::vector<OriginalImpactRecord> newImpactRecords;
    std::uint32_t invalidScalarDiagnostics=0;
    std::uint32_t frame0C92DE30=0;
    bool restoredValidRoadState=false;
    bool rivalAdvanced=false;
    OriginalBodyCollisionResult bodyCollision;
};

// Owns original player/rival state, shared contact scratch and input assets.
// Moving the owner keeps all internal path/collision/matrix references stable.
// A host calls tick once per original frame; rendering uses the exposed pose.
class OriginalDrivingSession {
public:
    // Experimental numerical rollback boundary. Checkpoints belong to this
    // exact reset; they are not wire packets and cannot cross session owners.
    // External audio/RNG callbacks must be absent: replaying them would repeat
    // sounds and mutate platform state outside this checkpoint.
    class Checkpoint {
    public:
        Checkpoint()=default;
        std::size_t memoryBytes() const;
    private:
        struct Data;
        std::shared_ptr<const Data> data_;
        friend class OriginalDrivingSession;
    };
    Checkpoint checkpoint() const;
    void restore(const Checkpoint& checkpoint);
    bool rollbackSafe() const;
    // Same-build diagnostic digest of numerical outputs and hidden histories.
    // Never used as network authority or as a cryptographic checksum.
    std::uint64_t rollbackDigest() const;
    OriginalDrivingSession();
    ~OriginalDrivingSession();
    OriginalDrivingSession(OriginalDrivingSession&&) noexcept;
    OriginalDrivingSession& operator=(OriginalDrivingSession&&) noexcept;
    OriginalDrivingSession(const OriginalDrivingSession&)=delete;
    OriginalDrivingSession& operator=(const OriginalDrivingSession&)=delete;

    // root is the native project root containing data/original_physics.
    // Pose uses original units/axes. All18 authored course/direction selections
    // use their original collision files. Invalid collision selections fail
    // explicitly. Failed loading leaves a previously valid session intact.
    // Returned digital-reset request must be consumed by the host input edge
    // adapter; numerical initialization cannot clear that separate host state.
    OriginalInitializationResult reset(const std::filesystem::path& root,const OriginalDrivingSelection& selection,
        const std::array<float,3>& originalPosition,const std::array<float,3>& originalAngles,const ImportedDrivingRoad* importedRoad=nullptr);
    // The owned elapsed-frame counter is authoritative; the matching legacy
    // input field is replaced before CEC0. Input gearEnabled is the host-owned
    // original actor bit15. No host steering or pedal smoothing occurs here.
    OriginalDrivingStepEffects tick(const OriginalVehicleInputs& inputs,
        const OriginalBodyCollisionResult* sharedContact=nullptr,
        const OriginalContactEngineOutput& frameEngineOutput={});
    //142860 runs inside contact completion, before its RNG/steering mask.
    // Bind after reset. The callback may consume this same driving seed.
    void setEngineOutput(OriginalContactEngineOutput output);
    // The outer scene invokes1415E0 after its race object update. This gives
    // tire selection the same shared seed, then preserves it for the next tick.
    void finishSoundFrame(const std::function<void(std::uint32_t&)>& output);
    // Result owners consume the same37C778 generator as driving and sound.
    // The outer owner supplies its original ordering at the scene boundary.
    void withSharedRandom(const std::function<void(std::uint32_t&)>& action);
    //159920 copies incomingFR4 to901650 and4004D8 to9015D4 each frame.
    // The outer race supplies these values; this owner does not derive a
    // host chase/boost policy. Defaults remain those supplied at reset.
    void setProgressCorrection(float progress0C901650,std::uint32_t mode0C4004D8);
    // Original067D00 requests actor+50 bit13 and byte2F4DE0; 068680/068520
    // clear that request. The next157AE0 contact preparation consumes it
    // into drive+1A8, preserving the original one-frame solver latency.
    void setRaceAutomaticBrake(bool enabled);
    //05BEE0 GO Init: set bit15 before the same frame's rules and solver.
    // Numeric mode0 also enables the rival's public actor; mode2 is solo.
    void enableRaceStart(std::uint32_t numericRaceMode);
    std::uint8_t raceAutomaticBrakeByte() const;
    // Original race transition observes actor bit14, which the numerical
    // solver publishes from its stopped latch. No host speed threshold.
    bool stoppedForRace() const;
    // Global platform frame and snapshot key are independent of elapsed race
    // frames. Default host policy starts frame0 and advances once per tick;
    // an outer original frame scheduler may supply its actual counter here.
    void setPlatformFrame(std::uint32_t frame0C92DE30,std::uint8_t digitalByte0C92ED00=0);
    bool ready() const noexcept;
    std::uint32_t platformFrame() const;
    const OriginalDrivingSelection& selection() const;
    const OriginalVehicleState& vehicle() const;
    const OriginalActorState& actor() const;
    const OriginalVehicleParameters& parameters() const;
    const OriginalPhysicsPath& path() const;
    const OriginalCollisionData& collision() const;
    const OriginalRoadContactState& roadContact() const;
    const OriginalContactCompletionState& contactCompletion() const;
    const OriginalWheelHistory& wheelHistory() const;
    const OriginalInitializationSideState& initializationSide() const;
    const OriginalPublishedActors& publishedActors() const;
    const OriginalRecoveryState& recovery() const;
    // Initialized refers to this reset; active is the current source+0 word.
    bool rivalInitialized() const;
    bool rivalActive() const;
    const OriginalRivalState& rivalActor() const;
    const OriginalActorState& rivalPublicActor() const;
    const OriginalRivalInitializationResult& rivalInitialization() const;
    const OriginalRivalPaceInputs& rivalPaceInputs() const;
    const OriginalRivalPath& rivalPath() const;
    const OriginalRivalRoadState& rivalRoadContact() const;
    std::uint32_t rivalFrameCounter() const;
    const OriginalBodyContactState& bodyContact() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Impl& state();
    const Impl& state() const;
};
} // namespace idas3::original

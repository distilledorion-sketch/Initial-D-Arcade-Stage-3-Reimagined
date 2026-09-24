using System;
using UnityEngine;

/// <summary>
/// Presentation-only reconstruction of a flexible hanging chain and a pendant.
/// Positions are sampled in metres at the native 60 Hz simulation tick. Native
/// forward=(sin(yaw),0,cos(yaw)); screen-right=(-cos(yaw),0,sin(yaw)).
/// Output is in the ornament viewport: +X right, -Y down, -Z toward the viewer.
/// This is not the original arcade game's recovered ornament simulation.
/// </summary>
public sealed class Idas3OrnamentMotion
{
    public const int ChainNodeCount = 9;
    public const float ChainLength = .9f;
    public const float FixedMountLength = .35f;
    public const float MaximumRollDegrees = 26f;
    public const float MaximumPitchDegrees = 18f;

    const double TickSeconds = 1.0 / 60.0;
    const double Degrees = 180.0 / Math.PI;
    const float Gravity = 9.80665f;
    const float StepSeconds = 1f / 240f;
    const float LinkLength = (ChainLength - FixedMountLength) / (ChainNodeCount - 1);
    const float ForceFilterSeconds = .055f;
    const float EndpointFilterSeconds = .06f;
    const float MaximumAcceleration = 32f;
    // Presentation gain compensates for the enlarged display geometry and
    // damped solver. Brief road jolts must bend the real links visibly, rather
    // than disappear as subpixel motion after the rigid-link skin is posed.
    const float VerticalImpulseGain = 2f;
    const int ConstraintIterations = 12;
    static readonly Vector3 Mount = new Vector3(0, -FixedMountLength, 0);

    // Reused storage: no allocations in sampling, integration or pose access.
    readonly Vector3[] points = new Vector3[ChainNodeCount];
    readonly Vector3[] previousPoints = new Vector3[ChainNodeCount];
    readonly Vector3[] poseBefore = new Vector3[ChainNodeCount];
    readonly Vector3[] renderPreviousPoints = new Vector3[ChainNodeCount];
    Quaternion renderPreviousRotation = Quaternion.identity;
    Vector3 filteredAcceleration, filteredEndpointAcceleration, endpointVelocity;
    double pitch, roll, twist, pitchVelocity, rollVelocity, twistVelocity;
    double previousX, previousY, previousZ, previousYaw, previousInterval;
    double velocityX, velocityY, velocityZ;
    ulong previousTick;
    bool ready, velocityReady, wasPaused;

    public Idas3OrnamentMotion() { Reset(); }

    /// <summary>Positive pitch moves a hanging pendant toward the viewer.</summary>
    public float PitchDegrees => (float)pitch;
    /// <summary>Positive roll moves a hanging pendant toward screen-right.</summary>
    public float RollDegrees => (float)roll;
    public Vector3 EulerDegrees => new Vector3(PitchDegrees, (float)twist, RollDegrees);
    public Quaternion Rotation => Quaternion.Euler(EulerDegrees);
    public Quaternion PendantRotation => Rotation;
    public Vector3 AttachmentPosition => points[ChainNodeCount - 1];
    public ulong Revision { get; private set; }
    public ulong PresentationRevision { get; private set; }
    public bool HasRenderMotion
    {
        get
        {
            if (!renderPreviousRotation.Equals(PendantRotation)) return true;
            for (int i = 0; i < ChainNodeCount; ++i)
                if (!renderPreviousPoints[i].Equals(points[i])) return true;
            return false;
        }
    }

    public Vector3 ChainPoint(int index)
    {
        if (index < 0 || index >= ChainNodeCount) throw new ArgumentOutOfRangeException(nameof(index));
        return points[index];
    }

    // Presentation reads the last two fixed simulation poses. It never feeds
    // interpolated positions back into the constrained solver or car physics.
    public Vector3 RenderChainPoint(int index, float alpha)
    {
        if (index < 0 || index >= ChainNodeCount) throw new ArgumentOutOfRangeException(nameof(index));
        return Vector3.Lerp(renderPreviousPoints[index], points[index], RenderAlpha(alpha));
    }

    public Quaternion RenderPendantRotation(float alpha)
        => Quaternion.Slerp(renderPreviousRotation, PendantRotation, RenderAlpha(alpha));

    static float RenderAlpha(float alpha) => Finite(alpha) ? Mathf.Clamp01(alpha) : 1f;

    void CaptureRenderPrevious()
    {
        bool changed = !renderPreviousRotation.Equals(PendantRotation);
        for (int i = 0; i < ChainNodeCount; ++i) changed |= !renderPreviousPoints[i].Equals(points[i]);
        Array.Copy(points, renderPreviousPoints, ChainNodeCount);
        renderPreviousRotation = PendantRotation;
        if (changed) ++PresentationRevision;
    }

    /// <summary>
    /// Samples actual car motion without changing driving physics. Duplicate
    /// ticks do nothing; pause freezes both links and pendant and resume takes
    /// a fresh velocity baseline. Invalid motion, respawns and long gaps reset.
    /// </summary>
    public void Sample(Vector3 position, float yawRadians, ulong simulationTick, bool active, bool paused)
    {
        if (!active || !Finite(position.x) || !Finite(position.y) || !Finite(position.z) || !Finite(yawRadians))
        {
            Reset();
            return;
        }
        if (paused) { wasPaused = true; return; }
        if (!ready || wasPaused) { Prime(position, yawRadians, simulationTick); return; }
        if (simulationTick == previousTick) return;
        if (simulationTick < previousTick || simulationTick - previousTick > 12)
        {
            Reset();
            Prime(position, yawRadians, simulationTick);
            return;
        }

        int elapsedTicks = (int)(simulationTick - previousTick);
        double interval = elapsedTicks * TickSeconds;
        double dx = (double)position.x - previousX, dy = (double)position.y - previousY, dz = (double)position.z - previousZ;
        double yawChange = WrapRadians(yawRadians - previousYaw);
        if (dx * dx + dy * dy + dz * dz > 150 * 150 * interval * interval || Math.Abs(yawChange) > Math.PI * .75)
        {
            Reset();
            Prime(position, yawRadians, simulationTick);
            return;
        }

        double vx = dx / interval, vy = dy / interval, vz = dz / interval;
        Vector3 acceleration = Vector3.zero;
        if (velocityReady)
        {
            // Velocity samples represent interval midpoints, including skips.
            double velocityInterval = (interval + previousInterval) * .5;
            double ax = (vx - velocityX) / velocityInterval;
            double ay = (vy - velocityY) / velocityInterval;
            double az = (vz - velocityZ) / velocityInterval;
            double sine = Math.Sin(yawRadians), cosine = Math.Cos(yawRadians);
            acceleration = new Vector3(
                (float)Clamp((-ax * cosine + az * sine) * .45, -MaximumAcceleration, MaximumAcceleration),
                (float)Clamp(ay * VerticalImpulseGain, -MaximumAcceleration, MaximumAcceleration),
                (float)Clamp((ax * sine + az * cosine) * .4, -MaximumAcceleration, MaximumAcceleration));
        }

        Array.Copy(points, poseBefore, ChainNodeCount);
        double oldPitch = pitch, oldRoll = roll, oldTwist = twist;
        CaptureRenderPrevious();
        // Preserve the chain's world direction while its vehicle frame turns.
        // It is not parented rigidly to each new car heading.
        RotateFrame((float)yawChange);
        float yawRate = (float)(yawChange / interval);
        for (int step = 0; step < elapsedTicks * 4; ++step)
        {
            // Even after a slow rendered frame, keep adjacent 60 Hz poses,
            // rather than interpolating across the entire skipped interval.
            if (step > 0 && step % 4 == 0) CaptureRenderPrevious();
            Advance(acceleration, yawRate);
        }
        if (PoseChanged(oldPitch, oldRoll, oldTwist)) { ++Revision; ++PresentationRevision; }

        previousX = position.x; previousY = position.y; previousZ = position.z;
        previousYaw = yawRadians; previousTick = simulationTick; previousInterval = interval;
        velocityX = vx; velocityY = vy; velocityZ = vz;
        velocityReady = true;
    }

    public void Reset()
    {
        bool changed = pitch != 0 || roll != 0 || twist != 0;
        for (int i = 0; i < ChainNodeCount; ++i)
        {
            Vector3 rest = new Vector3(0, -FixedMountLength - LinkLength * i, 0);
            changed |= points[i] != rest;
            points[i] = previousPoints[i] = poseBefore[i] = renderPreviousPoints[i] = rest;
        }
        pitch = roll = twist = pitchVelocity = rollVelocity = twistVelocity = 0;
        renderPreviousRotation = Quaternion.identity;
        filteredAcceleration = filteredEndpointAcceleration = endpointVelocity = Vector3.zero;
        previousX = previousY = previousZ = previousYaw = previousInterval = velocityX = velocityY = velocityZ = 0;
        previousTick = 0;
        ready = velocityReady = wasPaused = false;
        if (changed) ++Revision;
        ++PresentationRevision;
    }

    void Prime(Vector3 position, float yaw, ulong tick)
    {
        previousX = position.x; previousY = position.y; previousZ = position.z;
        previousYaw = yaw; previousTick = tick;
        previousInterval = velocityX = velocityY = velocityZ = 0;
        filteredAcceleration = Vector3.zero;
        ready = true;
        velocityReady = wasPaused = false;
        CaptureRenderPrevious();
    }

    void Advance(Vector3 acceleration, float yawRate)
    {
        float forceBlend = 1f - Mathf.Exp(-StepSeconds / ForceFilterSeconds);
        float previousVerticalAcceleration = filteredAcceleration.y;
        filteredAcceleration += (acceleration - filteredAcceleration) * forceBlend;
        // Preserve short suspension/road impulses instead of smoothing them
        // into an invisible gradual change in the chain's downward tension.
        filteredAcceleration.y = previousVerticalAcceleration + (acceleration.y - previousVerticalAcceleration) * (1f - Mathf.Exp(-StepSeconds / .028f));
        Vector3 apparentGravity = Vector3.down * Gravity - filteredAcceleration;
        Vector3 oldEndpoint = AttachmentPosition;
        for (int i = 1; i < ChainNodeCount; ++i)
        {
            Vector3 current = points[i];
            // A heavier pendant keeps moving as the light links catch up.
            float damping = Mathf.Exp(-(i == ChainNodeCount - 1 ? 1.3f : 2.1f) * StepSeconds);
            Vector3 displacement = Vector3.ClampMagnitude((current - previousPoints[i]) * damping, .028f);
            points[i] = current + displacement + apparentGravity * (StepSeconds * StepSeconds);
            previousPoints[i] = current;
        }

        SeedUnloadedBend(apparentGravity);
        for (int iteration = 0; iteration < ConstraintIterations; ++iteration)
        {
            points[0] = Mount;
            for (int i = 1; i < ChainNodeCount; ++i) ConstrainLink(i);
            for (int i = 1; i < ChainNodeCount; ++i) LimitTravel(i);
        }
        // A final root-to-tip projection guarantees no link can stretch after
        // the iterative solve. A chain becomes slack by bending its fixed-size
        // links; it cannot compress along a straight line like a rubber strap.
        points[0] = previousPoints[0] = Mount;
        for (int i = 1; i < ChainNodeCount; ++i)
        {
            Vector3 delta = points[i] - points[i - 1];
            float length = delta.magnitude;
            if (length > LinkLength) points[i] = points[i - 1] + delta * (LinkLength / length);
        }

        Vector3 newEndpointVelocity = (AttachmentPosition - oldEndpoint) / StepSeconds;
        Vector3 endpointAcceleration = Vector3.ClampMagnitude((newEndpointVelocity - endpointVelocity) / StepSeconds, 45f);
        endpointVelocity = newEndpointVelocity;
        float endpointBlend = 1f - Mathf.Exp(-StepSeconds / EndpointFilterSeconds);
        filteredEndpointAcceleration += (endpointAcceleration - filteredEndpointAcceleration) * endpointBlend;

        // The pendant has its own centre of mass and rotational inertia. It
        // responds to the acceleration of its attachment rather than copying
        // the last link's tangent; it can lag, rebound and twist independently.
        Vector3 attachmentAcceleration = filteredAcceleration + filteredEndpointAcceleration * .7f;
        double down = Math.Max(Gravity * .35, Gravity + attachmentAcceleration.y);
        double targetPitch = Clamp(Math.Atan2(attachmentAcceleration.z, down) * Degrees, -14, 14);
        double targetRoll = Clamp(-Math.Atan2(attachmentAcceleration.x, down) * Degrees, -21, 21);
        double targetTwist = Clamp(-yawRate * Degrees * .055 + endpointVelocity.x * 5, -9, 9);
        Spring(ref pitch, ref pitchVelocity, targetPitch, MaximumPitchDegrees, 8.3, .3);
        Spring(ref roll, ref rollVelocity, targetRoll, MaximumRollDegrees, 8.3, .3);
        Spring(ref twist, ref twistVelocity, targetTwist, 12, 5.2, .38);
    }

    void ConstrainLink(int index)
    {
        Vector3 delta = points[index] - points[index - 1];
        float distance = delta.magnitude;
        if (distance < .000001f)
        {
            distance = .000001f;
            delta = Vector3.down * distance;
        }
        if (Mathf.Abs(distance - LinkLength) < .0000001f) return;
        Vector3 correction = delta * ((distance - LinkLength) / distance);
        float parentWeight = index == 1 ? 0 : 1f;
        float childWeight = index == ChainNodeCount - 1 ? .14f : 1f;
        float inverseSum = 1f / (parentWeight + childWeight);
        points[index - 1] += correction * (parentWeight * inverseSum);
        points[index] -= correction * (childWeight * inverseSum);
    }

    void SeedUnloadedBend(Vector3 apparentGravity)
    {
        if (apparentGravity.y <= .1f) return;
        Vector3 end = AttachmentPosition - Mount;
        float length = end.magnitude;
        float compressed = ChainLength - FixedMountLength - length;
        if (compressed <= .000001f || length < .01f) return;
        Vector3 direction = end / length;
        for (int i = 1; i < ChainNodeCount - 1; ++i)
        {
            Vector3 offset = points[i] - Mount;
            if ((offset - direction * Vector3.Dot(offset, direction)).sqrMagnitude > .00000001f) return;
        }
        // A perfectly collinear mathematical chain cannot choose a direction
        // to buckle. The real pendant's off-axis centre of mass breaks that
        // symmetry. Seed the same small bias only when the chain is unloaded;
        // there is no idle noise, artificial periodic bob or spring extension.
        float amplitude = Mathf.Min(.007f, Mathf.Sqrt(compressed * length) * .64f);
        Vector3 bend = new Vector3(1, 0, .35f).normalized;
        for (int i = 1; i < ChainNodeCount - 1; ++i)
        {
            Vector3 offset = bend * (Mathf.Sin(Mathf.PI * i / (ChainNodeCount - 1)) * amplitude);
            points[i] += offset;
            previousPoints[i] += offset;
        }
    }

    void LimitTravel(int index)
    {
        // Cosmetic safety envelope for collisions or noisy source positions.
        // Normal motion does not touch it; the chain stays below its anchor.
        float reach = LinkLength * index;
        Vector3 point = points[index];
        point.y = Mathf.Min(point.y, Mount.y - reach * .65f);
        float ellipse = point.x * point.x / (reach * reach * .25f) + point.z * point.z / (reach * reach * .140625f);
        if (ellipse > 1)
        {
            float scale = 1f / Mathf.Sqrt(ellipse);
            point.x *= scale; point.z *= scale;
        }
        points[index] = point;
    }

    void RotateFrame(float yawChange)
    {
        if (Mathf.Abs(yawChange) < .0000001f) return;
        float sine = Mathf.Sin(yawChange), cosine = Mathf.Cos(yawChange);
        for (int i = 1; i < ChainNodeCount; ++i)
        {
            points[i] = RotateHorizontal(points[i], sine, cosine);
            previousPoints[i] = RotateHorizontal(previousPoints[i], sine, cosine);
        }
        endpointVelocity = RotateHorizontal(endpointVelocity, sine, cosine);
        filteredAcceleration = RotateHorizontal(filteredAcceleration, sine, cosine);
        filteredEndpointAcceleration = RotateHorizontal(filteredEndpointAcceleration, sine, cosine);
    }

    static Vector3 RotateHorizontal(Vector3 value, float sine, float cosine)
        => new Vector3(value.x * cosine + value.z * sine, value.y, -value.x * sine + value.z * cosine);

    bool PoseChanged(double oldPitch, double oldRoll, double oldTwist)
    {
        if (Math.Abs(pitch - oldPitch) > .000001 || Math.Abs(roll - oldRoll) > .000001 || Math.Abs(twist - oldTwist) > .000001) return true;
        for (int i = 0; i < ChainNodeCount; ++i)
            if ((points[i] - poseBefore[i]).sqrMagnitude > 1e-12f) return true;
        return false;
    }

    static void Spring(ref double angle, ref double angularVelocity, double target, double limit, double frequency, double damping)
    {
        angularVelocity += (frequency * frequency * (target - angle) - 2 * damping * frequency * angularVelocity) * StepSeconds;
        angle += angularVelocity * StepSeconds;
        if (angle > limit) { angle = limit; if (angularVelocity > 0) angularVelocity = 0; }
        if (angle < -limit) { angle = -limit; if (angularVelocity < 0) angularVelocity = 0; }
    }

    static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    static double Clamp(double value, double minimum, double maximum) => Math.Max(minimum, Math.Min(maximum, value));
    static double WrapRadians(double value) => Math.IEEERemainder(value, Math.PI * 2);
}

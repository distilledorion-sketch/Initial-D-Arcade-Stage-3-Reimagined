using System;
using UnityEngine;

// These checks use Unity's real vector/quaternion implementation and the same
// positions and simulation ticks consumed by the live ornament renderer.
public static class Idas3OrnamentMotionChecks
{
    public static void Run() => Debug.Log(RunChecks() + " " + RunViewChecks());

    sealed class Pose
    {
        internal readonly Vector3[] points = new Vector3[Idas3OrnamentMotion.ChainNodeCount];
        internal readonly Quaternion pendant;
        internal readonly ulong revision;
        internal Pose(Idas3OrnamentMotion motion)
        {
            for (int i = 0; i < points.Length; ++i) points[i] = motion.ChainPoint(i);
            pendant = motion.PendantRotation;
            revision = motion.Revision;
        }
    }

    static bool Finite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);
    static bool Finite(Vector3 v) => Finite(v.x) && Finite(v.y) && Finite(v.z);
    static bool Finite(Quaternion v) => Finite(v.x) && Finite(v.y) && Finite(v.z) && Finite(v.w);
    static float PoseDistance(Pose a, Pose b)
    {
        float distance = 0;
        for (int i = 0; i < a.points.Length; ++i)
            distance = Mathf.Max(distance, Vector3.Distance(a.points[i], b.points[i]));
        return distance;
    }
    static float Curvature(Idas3OrnamentMotion motion)
    {
        var root = motion.ChainPoint(0);
        var chord = motion.AttachmentPosition - root;
        if (chord.sqrMagnitude < 1e-8f) return 0;
        float distance = 0;
        for (int i = 1; i < Idas3OrnamentMotion.ChainNodeCount - 1; ++i)
        {
            var offset = motion.ChainPoint(i) - root;
            distance = Mathf.Max(distance, (offset - Vector3.Project(offset, chord)).magnitude);
        }
        return distance;
    }
    static float PendantLag(Idas3OrnamentMotion motion)
    {
        int last = Idas3OrnamentMotion.ChainNodeCount - 1;
        var tangent = motion.ChainPoint(last) - motion.ChainPoint(last - 1);
        return tangent.sqrMagnitude > 1e-8f
            ? Vector3.Angle(tangent, motion.PendantRotation * Vector3.down) : 0;
    }

    public static string RunViewChecks()
    {
        int checks = 0;
        foreach (float yaw in new[] { 0f, Mathf.PI * .5f, Mathf.PI, -Mathf.PI * .5f })
        foreach (float direction in new[] { -1f, 1f })
        {
            var forward = new Vector3(Mathf.Sin(yaw), 0, Mathf.Cos(yaw));
            // Native XMMatrixLookAtRH uses this horizontal row. The ornament
            // camera looks along Unity +Z, and its texture X is not mirrored.
            var nativeScreenRight = Vector3.Cross(forward, Vector3.up).normalized;
            var motion = new Idas3OrnamentMotion();
            for (ulong tick = 0; tick <= 120; ++tick)
            {
                float t = tick / 60f;
                motion.Sample(forward * (30 * t) + nativeScreenRight * (.5f * 6 * direction * t * t), yaw, tick, true, false);
            }
            ++checks;
            if (motion.AttachmentPosition.x * direction >= -.025f)
                throw new Exception("Ornament chain bends toward native screen acceleration, rather than outward.");
            ++checks;
            if ((motion.PendantRotation * Vector3.down).x * direction >= -.025f)
                throw new Exception("Ornament pendant rotates toward native screen acceleration, rather than outward.");
            ++checks;
            if (Mathf.Abs(motion.AttachmentPosition.z) > .005f)
                throw new Exception("Native horizontal camera-basis conversion introduces forward/backward chain motion.");
        }
        return "PASS ornament camera basis: " + checks + " RH-view acceleration/Unity chain and pendant checks.";
    }

    public static string RunChecks()
    {
        int checks = 0;
        void Check(bool condition, string message)
        {
            ++checks;
            if (!condition) throw new Exception("Ornament motion: " + message);
        }
        void Geometry(Idas3OrnamentMotion sample, string context)
        {
            var root = new Vector3(0, -Idas3OrnamentMotion.FixedMountLength, 0);
            Check(Vector3.Distance(sample.ChainPoint(0), root) < .000001f, context + ": mount is not pinned");
            float linkLength = (Idas3OrnamentMotion.ChainLength - Idas3OrnamentMotion.FixedMountLength)
                / (Idas3OrnamentMotion.ChainNodeCount - 1);
            for (int i = 0; i < Idas3OrnamentMotion.ChainNodeCount; ++i)
            {
                Check(Finite(sample.ChainPoint(i)), context + ": non-finite chain point");
                if (i > 0)
                    Check(Vector3.Distance(sample.ChainPoint(i), sample.ChainPoint(i - 1)) <= linkLength + .0001f,
                        context + ": a chain link stretches beyond its length");
            }
            Check(sample.AttachmentPosition.Equals(sample.ChainPoint(Idas3OrnamentMotion.ChainNodeCount - 1)),
                context + ": pendant attachment is detached from the final link");
            Check(Finite(sample.PendantRotation) && Finite(sample.PitchDegrees) && Finite(sample.RollDegrees), context + ": non-finite pendant pose");
            float quaternionLength = Mathf.Sqrt(Quaternion.Dot(sample.PendantRotation, sample.PendantRotation));
            Check(Mathf.Abs(quaternionLength - 1) < .0001f, context + ": pendant rotation is not normalized");
            Check(Mathf.Abs(sample.PitchDegrees) <= Idas3OrnamentMotion.MaximumPitchDegrees + .001f
                && Mathf.Abs(sample.RollDegrees) <= Idas3OrnamentMotion.MaximumRollDegrees + .001f,
                context + ": pendant escaped its visible angle limits");
        }
        void Rest(Idas3OrnamentMotion sample, string context, float positionTolerance = .00001f, float angleTolerance = .01f)
        {
            for (int i = 0; i < Idas3OrnamentMotion.ChainNodeCount; ++i)
            {
                float y = Mathf.Lerp(-Idas3OrnamentMotion.FixedMountLength, -Idas3OrnamentMotion.ChainLength,
                    i / (float)(Idas3OrnamentMotion.ChainNodeCount - 1));
                Check(Vector3.Distance(sample.ChainPoint(i), new Vector3(0, y, 0)) < positionTolerance,
                    context + ": chain did not return to rest");
            }
            Check(Quaternion.Angle(sample.PendantRotation, Quaternion.identity) <= angleTolerance,
                context + ": pendant did not return to rest");
        }
        void Frozen(Idas3OrnamentMotion sample, Pose previous, string context)
        {
            var next = new Pose(sample);
            Check(next.revision == previous.revision, context + ": renderer revision changed");
            Check(PoseDistance(next, previous) == 0 && next.pendant.Equals(previous.pendant), context + ": pose changed");
        }

        var motion = new Idas3OrnamentMotion();
        Rest(motion, "new ornament");
        for (ulong tick = 0; tick <= 600; ++tick)
            motion.Sample(new Vector3(100, 3, tick * .5f), 0, tick, true, false);
        Rest(motion, "steady straight driving", .0001f, .02f);
        Geometry(motion, "steady straight driving");

        Pose Accelerate(float acceleration)
        {
            motion.Reset();
            for (ulong tick = 0; tick <= 120; ++tick)
            {
                float t = tick / 60f;
                motion.Sample(new Vector3(0, 0, 40 * t + .5f * acceleration * t * t), 0, tick, true, false);
            }
            Geometry(motion, "acceleration/braking");
            return new Pose(motion);
        }
        var accelerate = Accelerate(6);
        var brake = Accelerate(-6);
        int end = Idas3OrnamentMotion.ChainNodeCount - 1;
        Check(accelerate.points[end].z < -.025f && brake.points[end].z > .025f, "acceleration and braking do not deflect the chain in opposite directions");
        Check(Mathf.Abs(accelerate.points[end].z + brake.points[end].z) < .003f
            && Mathf.Abs(accelerate.points[end].x) < .003f && Mathf.Abs(brake.points[end].x) < .003f,
            "straight acceleration creates asymmetric or lateral chain motion");
        Check((accelerate.pendant * Vector3.down).z < -.025f && (brake.pendant * Vector3.down).z > .025f,
            "acceleration and braking do not rotate the pendant in opposite directions");

        Pose Turn(float direction, int framesPerSecond)
        {
            motion.Reset();
            for (int frame = 0; frame <= framesPerSecond * 2; ++frame)
            {
                ulong tick = (ulong)(frame * 60 / framesPerSecond);
                double t = tick / 60.0, angle = t * .3 * direction;
                motion.Sample(new Vector3((float)(-80 * (1 - Math.Cos(angle)) * direction), 0, (float)(80 * Math.Sin(angle) * direction)),
                    (float)-angle, tick, true, false);
            }
            return new Pose(motion);
        }
        var right = Turn(1, 60);
        var left = Turn(-1, 60);
        Check(right.points[end].x < -.04f && left.points[end].x > .04f, "left and right turns do not bend the chain outward");
        Check(Mathf.Abs(right.points[end].x + left.points[end].x) < .003f, "left/right chain deflection is asymmetric");
        var fps30 = Turn(1, 30);
        var fps60 = Turn(1, 60);
        var fps120 = Turn(1, 120);
        Check(PoseDistance(fps30, fps60) < .035f && Quaternion.Angle(fps30.pendant, fps60.pendant) < 3,
            "constant-radius turn depends excessively on 30/60 Hz rendering");
        Check(PoseDistance(fps60, fps120) == 0 && fps60.pendant.Equals(fps120.pendant), "repeated ticks at 120 Hz changed the pose");

        // A smooth, brief change in road height supplies the vertical impulse.
        // Once the car is level, movement must rebound and die out by itself.
        motion.Reset();
        float minimumY = float.PositiveInfinity, maximumY = float.NegativeInfinity;
        float previousY = -Idas3OrnamentMotion.ChainLength;
        bool roseAfterImpulse = false, fellAfterImpulse = false;
        float postInputTravel = 0;
        for (ulong tick = 0; tick <= 300; ++tick)
        {
            float t = tick / 60f;
            float phase = Mathf.Clamp01((t - .5f) / .22f);
            float height = .08f * Mathf.Pow(Mathf.Sin(Mathf.PI * phase), 2);
            motion.Sample(new Vector3(0, height, 25 * t), 0, tick, true, false);
            Geometry(motion, "road bump at tick " + tick);
            float y = motion.AttachmentPosition.y;
            if (t >= .5f)
            {
                minimumY = Mathf.Min(minimumY, y);
                maximumY = Mathf.Max(maximumY, y);
                roseAfterImpulse |= y - previousY > .00005f;
                fellAfterImpulse |= previousY - y > .00005f;
            }
            if (t >= .75f) postInputTravel += Mathf.Abs(y - previousY);
            previousY = y;
        }
        Check(maximumY - minimumY > .002f, "road-height impulse does not produce visible vertical bounce");
        Check(roseAfterImpulse && fellAfterImpulse, "vertical chain movement does not lift and rebound after a road-height impulse");
        Check(postInputTravel > .0001f, "vertical chain movement stops with the car-height input instead of responding to its momentum");
        for (ulong tick = 301; tick <= 1080; ++tick)
            motion.Sample(new Vector3(0, 0, tick / 60f * 25), 0, tick, true, false);
        Rest(motion, "road bump settling", .01f, .6f);

        // A flexible chain must curve in a transient; merely rotating every
        // chain vertex with one pendulum quaternion cannot pass these checks.
        motion.Reset();
        float maxCurvature = 0, maxLag = 0;
        for (ulong tick = 0; tick <= 240; ++tick)
        {
            float t = tick / 60f;
            var position = new Vector3(.9f * Mathf.Sin(4 * t), .03f * Mathf.Sin(5 * t), 25 * t);
            motion.Sample(position, 0, tick, true, false);
            Geometry(motion, "changing acceleration at tick " + tick);
            maxCurvature = Mathf.Max(maxCurvature, Curvature(motion));
            maxLag = Mathf.Max(maxLag, PendantLag(motion));
        }
        Check(maxCurvature > .001f, "changing acceleration leaves the entire chain rigid/collinear");
        Check(maxLag > 1, "pendant cannot lag independently of the chain's last link");

        Pose[] Transient(int framesPerSecond)
        {
            var sample = new Idas3OrnamentMotion();
            var values = new Pose[181];
            for (int frame = 0; frame <= framesPerSecond * 6; ++frame)
            {
                ulong tick = (ulong)(frame * 60 / framesPerSecond);
                double t = tick / 60.0;
                sample.Sample(new Vector3((float)(2 * Math.Sin(t * 2)), (float)(.03 * Math.Sin(t * 3)),
                    (float)(25 * t + 3 * Math.Sin(t * 1.3))), 0, tick, true, false);
                if (tick % 2 == 0) values[(int)tick / 2] = new Pose(sample);
            }
            return values;
        }
        var transient30 = Transient(30);
        var transient60 = Transient(60);
        var transient120 = Transient(120);
        float maxCadenceDistance = 0, maxCadenceAngle = 0;
        for (int i = 0; i < transient30.Length; ++i)
        {
            float distance = PoseDistance(transient30[i], transient60[i]);
            float angle = Quaternion.Angle(transient30[i].pendant, transient60[i].pendant);
            maxCadenceDistance = Mathf.Max(maxCadenceDistance, distance);
            maxCadenceAngle = Mathf.Max(maxCadenceAngle, angle);
            Check(distance < .035f && angle < 3, "changing acceleration depends excessively on render cadence at sample " + i);
            Check(PoseDistance(transient60[i], transient120[i]) == 0 && transient60[i].pendant.Equals(transient120[i].pendant),
                "changing acceleration reacts to repeated native ticks at sample " + i);
        }

        var beforePause = new Pose(motion);
        motion.Sample(new Vector3(1234, 0, 5678), 2, 240, true, false);
        Frozen(motion, beforePause, "duplicate tick");
        for (ulong tick = 241; tick <= 1000; ++tick)
            motion.Sample(new Vector3(1000, 20, 1000), 1, tick, true, true);
        Frozen(motion, beforePause, "pause");
        motion.Sample(new Vector3(1000, 20, 1000), 1, 1000, true, false);
        Frozen(motion, beforePause, "resume baseline");
        for (ulong tick = 1001; tick <= 1720; ++tick)
            motion.Sample(new Vector3(1000, 20, 1000), 1, tick, true, false);
        Rest(motion, "released ornament settling", .01f, .6f);

        void Excite() => Turn(1, 60);
        Excite(); motion.Sample(Vector3.zero, 0, 121, false, false); Rest(motion, "frontend reset");
        Excite(); motion.Sample(Vector3.zero, 0, 1, true, false); Rest(motion, "race tick rewind");
        Excite(); motion.Sample(Vector3.zero, 0, 200, true, false); Rest(motion, "long hitch");
        Excite(); motion.Sample(new Vector3(9999, 0, 0), 0, 121, true, false); Rest(motion, "horizontal teleport");
        Excite(); motion.Sample(new Vector3(0, 9999, 0), 0, 121, true, false); Rest(motion, "vertical teleport");
        motion.Reset();
        motion.Sample(Vector3.zero, Mathf.PI - .001f, 0, true, false);
        motion.Sample(new Vector3(0, 0, -.5f), -Mathf.PI + .001f, 1, true, false);
        motion.Sample(new Vector3(0, 0, -1), -Mathf.PI + .001f, 2, true, false);
        Rest(motion, "heading crosses +/- pi", .0001f, .02f);
        foreach (float invalid in new[] { float.NaN, float.PositiveInfinity, float.NegativeInfinity })
        {
            foreach (var position in new[] { new Vector3(invalid, 0, 0), new Vector3(0, invalid, 0), new Vector3(0, 0, invalid) })
            {
                Excite(); motion.Sample(position, 0, 121, true, false); Rest(motion, "invalid car position");
            }
            Excite(); motion.Sample(Vector3.zero, invalid, 121, true, false); Rest(motion, "invalid car heading");
        }

        motion.Reset();
        for (ulong tick = 0; tick <= 1800; ++tick)
        {
            float x = ((tick / 15) % 2 == 0 ? -1 : 1) * .8f;
            float y = .12f * Mathf.Sin(tick * .4f);
            motion.Sample(new Vector3(x, y, tick * .5f), 0, tick, true, false);
            Geometry(motion, "repeated impacts at tick " + tick);
        }
        motion.Reset(); Rest(motion, "explicit car/ornament reset");
        return "PASS flexible ornament motion: " + checks + " checks; pinned mount, bounded links, curvature "
            + maxCurvature.ToString("F4") + " m, vertical rebound " + (maximumY - minimumY).ToString("F4")
            + " m, independent pendant lag " + maxLag.ToString("F2") + " degrees; maximum 30/60 Hz difference "
            + maxCadenceDistance.ToString("F4") + " m / " + maxCadenceAngle.ToString("F2")
            + " degrees; duplicate ticks, pause/resume, settling, resets and finite bounds.";
    }
}

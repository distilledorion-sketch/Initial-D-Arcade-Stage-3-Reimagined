using System;
using UnityEngine;

// Presentation interpolation must add render frames without adding, delaying or
// mutating native physics samples. Run in Unity with its real quaternion math.
public static class Idas3OrnamentInterpolationChecks
{
    public static void Run() => Debug.Log(RunChecks());

    sealed class Pose
    {
        internal readonly Vector3[] points = new Vector3[Idas3OrnamentMotion.ChainNodeCount];
        internal readonly Quaternion pendant;
        internal readonly ulong revision, presentationRevision;
        internal Pose(Idas3OrnamentMotion motion, float alpha, bool rendered = true)
        {
            for (int i = 0; i < points.Length; ++i)
                points[i] = rendered ? motion.RenderChainPoint(i, alpha) : motion.ChainPoint(i);
            pendant = rendered ? motion.RenderPendantRotation(alpha) : motion.PendantRotation;
            revision = motion.Revision;
            presentationRevision = motion.PresentationRevision;
        }
    }

    static bool Finite(float value) => !float.IsNaN(value) && !float.IsInfinity(value);
    static bool Finite(Vector3 value) => Finite(value.x) && Finite(value.y) && Finite(value.z);
    static bool Finite(Quaternion value) => Finite(value.x) && Finite(value.y) && Finite(value.z) && Finite(value.w);
    static float Distance(Pose a, Pose b)
    {
        float distance = 0;
        for (int i = 0; i < a.points.Length; ++i)
            distance = Mathf.Max(distance, Vector3.Distance(a.points[i], b.points[i]));
        return distance;
    }
    static float QuaternionDistance(Quaternion a, Quaternion b)
    {
        float sign = Quaternion.Dot(a, b) < 0 ? -1 : 1;
        return Mathf.Max(Mathf.Abs(a.x - sign * b.x), Mathf.Abs(a.y - sign * b.y),
            Mathf.Abs(a.z - sign * b.z), Mathf.Abs(a.w - sign * b.w));
    }
    static Vector3 Path(ulong tick)
    {
        double seconds = tick / 60.0;
        return new Vector3((float)(1.8 * Math.Sin(seconds * 2)), (float)(.015 * Math.Sin(seconds * 3)),
            (float)(20 * seconds + 2 * Math.Sin(seconds * 1.3)));
    }

    public static string RunChecks()
    {
        int checks = 0, changingSubframes = 0;
        void Check(bool condition, string message)
        {
            ++checks;
            if (!condition) throw new Exception("Ornament interpolation: " + message);
        }
        void Equal(Pose a, Pose b, string context, float positionTolerance = .000001f, float rotationTolerance = .000001f)
        {
            Check(Distance(a, b) <= positionTolerance, context + ": chain points differ");
            Check(QuaternionDistance(a.pendant, b.pendant) <= rotationTolerance, context + ": pendant rotations differ");
        }
        void PairCollapsed(Idas3OrnamentMotion motion, string context)
        {
            var actual = new Pose(motion, 1, false);
            foreach (float alpha in new[] { 0f, .25f, .5f, .75f, 1f })
                Equal(new Pose(motion, alpha), actual, context);
        }

        var sample = new Idas3OrnamentMotion();
        PairCollapsed(sample, "initial pose");
        sample.Sample(Path(0), 0, 0, true, false);
        PairCollapsed(sample, "initial position baseline");
        int changingPairs = 0;
        for (ulong tick = 1; tick <= 180; ++tick)
        {
            var previous = new Pose(sample, 1, false);
            sample.Sample(Path(tick), 0, tick, true, false);
            var current = new Pose(sample, 1, false);
            var renderedPrevious = new Pose(sample, 0);
            var renderedCurrent = new Pose(sample, 1);
            Equal(renderedPrevious, previous, "alpha zero at tick " + tick);
            Equal(renderedCurrent, current, "alpha one at tick " + tick);
            var middle = new Pose(sample, .5f);
            for (int i = 0; i < middle.points.Length; ++i)
                Check(Vector3.Distance(middle.points[i], Vector3.Lerp(previous.points[i], current.points[i], .5f)) < .000001f,
                    "chain midpoint does not blend the two actual solver poses");
            Check(QuaternionDistance(middle.pendant, Quaternion.Slerp(previous.pendant, current.pendant, .5f)) < .000001f,
                "pendant midpoint does not interpolate actual orientation");
            if (Distance(previous, current) > .00001f)
            {
                ++changingPairs;
                Check(Distance(previous, middle) > .000001f && Distance(current, middle) > .000001f,
                    "intermediate render frame snaps to an endpoint");
            }

            foreach (float alpha in new[] { 0f, .1f, .25f, .5f, .75f, .9f, 1f })
            {
                var rendered = new Pose(sample, alpha);
                Check(rendered.revision == current.revision, "render sampling changed solver revision");
                Check(rendered.presentationRevision == current.presentationRevision, "render sampling changed presentation revision");
                Check(Finite(rendered.pendant) && Mathf.Abs(Quaternion.Dot(rendered.pendant, rendered.pendant) - 1) < .00001f,
                    "rendered pendant orientation is non-finite or not normalized");
                Check(Vector3.Distance(rendered.points[0], new Vector3(0, -Idas3OrnamentMotion.FixedMountLength, 0)) < .000001f,
                    "interpolation moves the pinned mount");
                for (int i = 0; i < rendered.points.Length; ++i)
                {
                    Check(Finite(rendered.points[i]), "rendered chain contains non-finite coordinates");
                    var low = Vector3.Min(previous.points[i], current.points[i]);
                    var high = Vector3.Max(previous.points[i], current.points[i]);
                    var p = rendered.points[i];
                    Check(p.x >= low.x - .000001f && p.x <= high.x + .000001f
                        && p.y >= low.y - .000001f && p.y <= high.y + .000001f
                        && p.z >= low.z - .000001f && p.z <= high.z + .000001f,
                        "interpolation overshoots the chain's solver positions");
                }
            }
            Equal(new Pose(sample, 1, false), current, "render sampling mutates solver state");
            Check(sample.Revision == current.revision, "render sampling advances physics");
            Check(sample.PresentationRevision == current.presentationRevision, "render sampling advances presentation history");

            // The native tick is repeated at high display rates. An unrelated
            // incoming position must neither advance nor erase the blend pair.
            sample.Sample(new Vector3(999, 999, 999), 1, tick, true, false);
            Equal(new Pose(sample, 0), renderedPrevious, "duplicate tick erases previous endpoint");
            Equal(new Pose(sample, 1), renderedCurrent, "duplicate tick changes current endpoint");
            Check(sample.Revision == current.revision, "duplicate tick advances interpolation revision");
            Check(sample.PresentationRevision == current.presentationRevision, "duplicate tick changes interpolation history");
        }
        Check(changingPairs > 100, "test trajectory did not exercise moving interpolation pairs");

        var first = new Pose(sample, 0);
        var last = new Pose(sample, 1);
        foreach (float alpha in new[] { -100f, -.5f }) Equal(new Pose(sample, alpha), first, "negative alpha clamp");
        foreach (float alpha in new[] { 1.5f, 100f, float.NaN, float.PositiveInfinity, float.NegativeInfinity })
            Equal(new Pose(sample, alpha), last, "large/non-finite alpha fallback");

        // A 30 FPS frame advances two native ticks. Its previous endpoint must
        // be the penultimate native pose, not the entire prior rendered frame.
        sample.Reset();
        for (ulong tick = 0; tick <= 8; ++tick)
        {
            float t = tick / 60f;
            sample.Sample(new Vector3(3 * t * t, 0, 20 * t), 0, tick, true, false);
        }
        var beforeGap = new Pose(sample, 1, false);
        float afterTime = 10 / 60f;
        sample.Sample(new Vector3(3 * afterTime * afterTime, 0, 20 * afterTime), 0, 10, true, false);
        var gapPrevious = new Pose(sample, 0);
        var afterGap = new Pose(sample, 1, false);
        float fullMovement = Distance(beforeGap, afterGap);
        Check(fullMovement > .00001f, "two-tick fixture did not excite the chain");
        Check(Distance(gapPrevious, afterGap) < fullMovement * .85f
            && Distance(gapPrevious, beforeGap) > fullMovement * .1f,
            "two-tick frame retains the old rendered frame instead of one previous native tick");

        Pose[] Cadence(int framesPerSecond)
        {
            var motion = new Idas3OrnamentMotion();
            var poses = new Pose[241];
            ulong previousTick = ulong.MaxValue;
            Pose previousRendered = null;
            for (int frame = 0; frame <= framesPerSecond * 4; ++frame)
            {
                double nativeTime = frame * 60.0 / framesPerSecond;
                ulong tick = (ulong)Math.Floor(nativeTime);
                float alpha = (float)(nativeTime - tick);
                motion.Sample(Path(tick), 0, tick, true, false);
                var actual = new Pose(motion, 1, false);
                var rendered = new Pose(motion, alpha);
                Equal(new Pose(motion, 1, false), actual, framesPerSecond + " FPS presentation changed solver state");
                Check(motion.Revision == actual.revision, framesPerSecond + " FPS presentation changed solver revision");
                Check(motion.PresentationRevision == actual.presentationRevision, framesPerSecond + " FPS render reads changed presentation revision");
                if (previousTick == tick && previousRendered != null && Distance(previousRendered, rendered) > .000001f)
                    ++changingSubframes;
                previousTick = tick;
                previousRendered = rendered;
                poses[(int)tick] = actual;
            }
            return poses;
        }
        var reference = Cadence(60);
        float maximumCadenceDistance = 0, maximumCadenceAngle = 0;
        foreach (int fps in new[] { 30, 120, 144, 240 })
        {
            int subframesBefore = changingSubframes;
            var values = Cadence(fps);
            for (int tick = 0; tick < values.Length; ++tick)
            {
                if (values[tick] == null) continue;
                float distance = Distance(values[tick], reference[tick]);
                float angle = Quaternion.Angle(values[tick].pendant, reference[tick].pendant);
                maximumCadenceDistance = Mathf.Max(maximumCadenceDistance, distance);
                maximumCadenceAngle = Mathf.Max(maximumCadenceAngle, angle);
                if (fps == 30)
                    Check(distance < .035f && angle < 3, "30 FPS endpoints differ excessively from 60 FPS solver motion");
                else
                    Equal(values[tick], reference[tick], fps + " FPS native endpoint at tick " + tick);
            }
            if (fps > 60)
                Check(changingSubframes - subframesBefore > 50, fps + " FPS did not produce distinct render poses between native ticks");
        }

        // Pause keeps the visible physics pose frozen. Resuming and all fresh
        // baselines collapse old interpolation history, preventing a trail
        // from the last race/car/location from blending into the new frame.
        void Excite()
        {
            sample.Reset();
            for (ulong tick = 0; tick <= 90; ++tick) sample.Sample(Path(tick), 0, tick, true, false);
        }
        Excite();
        var pausedActual = new Pose(sample, 1, false);
        sample.Sample(new Vector3(90, 10, 50), 1, 900, true, true);
        Equal(new Pose(sample, 1, false), pausedActual, "pause changes physical pose");
        Check(sample.Revision == pausedActual.revision, "pause changes solver revision");
        Check(sample.PresentationRevision == pausedActual.presentationRevision, "pause changes presentation revision");
        sample.Sample(new Vector3(90, 10, 50), 1, 900, true, false);
        PairCollapsed(sample, "resume baseline");
        Equal(new Pose(sample, 1, false), pausedActual, "resume changes frozen physical pose");
        Excite(); sample.Reset(); PairCollapsed(sample, "explicit reset");
        Excite(); sample.Sample(Vector3.zero, 0, 91, false, false); PairCollapsed(sample, "inactive presentation");
        Excite(); sample.Sample(new Vector3(9999, 0, 0), 0, 91, true, false); PairCollapsed(sample, "teleport");
        Excite(); sample.Sample(Path(1), 0, 1, true, false); PairCollapsed(sample, "race tick rewind");
        Excite(); sample.Sample(Path(200), 0, 200, true, false); PairCollapsed(sample, "long native tick gap");
        Excite(); sample.Sample(new Vector3(float.NaN, 0, 0), 0, 91, true, false); PairCollapsed(sample, "invalid source position");
        return "PASS ornament render interpolation: " + checks + " checks; actual endpoints and midpoints, alpha bounds, pinned mount, "
            + "unchanged simulation, one-tick history after skipped frames, pause/reset baselines; " + changingSubframes
            + " distinct high-FPS subframes; maximum cadence endpoint difference " + maximumCadenceDistance.ToString("F5")
            + " m / " + maximumCadenceAngle.ToString("F3") + " degrees.";
    }
}

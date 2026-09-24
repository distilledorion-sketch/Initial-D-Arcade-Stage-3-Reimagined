using System;
using UnityEngine;

// Recovered glTF skin data, with the exact primitive/vertex order used by ORN1.
// The simulated motion is new; the bind positions and weights are source data.
public static class Idas3OrnamentChainRig
{
    [Serializable] public sealed class Rig
    {
        public int version, strapId, fixedJoint;
        public Vector3[] joints = Array.Empty<Vector3>();
        public string[] jointNames = Array.Empty<string>();
        public Part[] parts = Array.Empty<Part>();
    }

    [Serializable] public sealed class Part
    {
        public Weight[] weights = Array.Empty<Weight>();
    }

    [Serializable] public sealed class Weight
    {
        public int[] joints = Array.Empty<int>();
        public float[] weights = Array.Empty<float>();
    }

    public static Rig Load(int strapId)
    {
        if (strapId < 1 || strapId > 4) return null;
        var asset = Resources.Load<TextAsset>("ArcadeOrnaments/Rigs/strap_" + strapId.ToString("D4"));
        if (!asset) return null;
        try
        {
            var rig = JsonUtility.FromJson<Rig>(asset.text);
            if (rig == null || rig.version != 1 || rig.strapId != strapId || rig.joints == null
                || rig.jointNames == null || rig.joints.Length != rig.jointNames.Length
                || rig.joints.Length < 2 || rig.joints.Length > 128 || rig.fixedJoint < 0
                || rig.fixedJoint >= rig.joints.Length || rig.parts == null || rig.parts.Length < 1)
                throw new FormatException("Invalid recovered chain rig header.");
            foreach (var joint in rig.joints)
                if (!Finite(joint.x) || !Finite(joint.y) || !Finite(joint.z))
                    throw new FormatException("Invalid recovered chain bind position.");
            foreach (var part in rig.parts)
            {
                if (part?.weights == null || part.weights.Length < 3)
                    throw new FormatException("Invalid recovered chain rig part.");
                foreach (var vertex in part.weights)
                {
                    if (vertex?.joints == null || vertex.weights == null
                        || vertex.joints.Length != 4 || vertex.weights.Length != 4)
                        throw new FormatException("Invalid recovered chain skin influences.");
                    float sum = 0;
                    for (int i = 0; i < 4; i++)
                    {
                        float weight = vertex.weights[i];
                        if (vertex.joints[i] < 0 || vertex.joints[i] >= rig.joints.Length
                            || !Finite(weight) || weight < 0 || weight > 1)
                            throw new FormatException("Invalid recovered chain skin weight.");
                        sum += weight;
                    }
                    if (Mathf.Abs(sum - 1) > .001f)
                        throw new FormatException("Recovered chain skin weights are not normalized.");
                }
            }
            return rig;
        }
        catch (Exception error)
        {
            Debug.LogError("Could not load ornament chain rig " + strapId + ": " + error.Message);
            return null;
        }
        finally { Resources.UnloadAsset(asset); }
    }

    static bool Finite(float value) { return !float.IsNaN(value) && !float.IsInfinity(value); }
}

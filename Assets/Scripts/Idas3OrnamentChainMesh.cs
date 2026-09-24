using System;
using UnityEngine;

// Deform the recovered strap with its original skin weights. Metal links have
// one joint influence and stay rigid; cord vertices blend neighboring joints.
// The selected MeshParts retain ownership of the mesh.
internal sealed class Idas3OrnamentChainMesh
{
    readonly Mesh mesh;
    readonly Vector3[] rest, normals, vertices, deformedNormals, joints, posedJoints;
    readonly Idas3OrnamentChainRig.Weight[] weights;
    readonly Matrix4x4[] transforms;
    readonly Quaternion[] rotations;
    readonly int fixedJoint, attachmentJoint;
    static readonly Vector3 RestAttachment = new Vector3(0, -Idas3OrnamentMotion.ChainLength, 0);
    bool deformed;
    internal Vector3 AttachmentPosition { get; private set; } = RestAttachment;

    internal Idas3OrnamentChainMesh(Mesh mesh, Matrix4x4 assembly, Idas3OrnamentChainRig.Rig rig, int part)
    {
        this.mesh=mesh;fixedJoint=rig.fixedJoint;
        rest=mesh.vertices;normals=mesh.normals;
        vertices=new Vector3[rest.Length];deformedNormals=new Vector3[rest.Length];
        weights=rig.parts[part].weights;
        if(weights.Length!=rest.Length)throw new InvalidOperationException("Chain skin does not match recovered mesh vertices.");
        joints=new Vector3[rig.joints.Length];posedJoints=new Vector3[joints.Length];
        transforms=new Matrix4x4[joints.Length];rotations=new Quaternion[joints.Length];
        for(int i=0;i<joints.Length;++i)joints[i]=assembly.MultiplyPoint3x4(rig.joints[i]);
        // Both material parts must report the same final ring attachment. The
        // last palette joint is a terminal socket, not necessarily skinned.
        attachmentJoint=fixedJoint;
        foreach(var rigPart in rig.parts)
            foreach(var binding in rigPart.weights)
                for(int i=0;i<binding.weights.Length;++i)
                    if(binding.weights[i]>0)attachmentJoint=Mathf.Max(attachmentJoint,binding.joints[i]);
        for(int i=0;i<rest.Length;++i){rest[i]=assembly.MultiplyPoint3x4(rest[i]);normals[i]=assembly.MultiplyVector(normals[i]).normalized;}
        mesh.MarkDynamic();mesh.vertices=rest;mesh.normals=normals;
        mesh.bounds=new Bounds(new Vector3(0,-.7f,0),Vector3.one*4);
    }

    internal void Restore()
    {
        AttachmentPosition=RestAttachment;
        if(!deformed)return;
        mesh.vertices=rest;mesh.normals=normals;deformed=false;
    }

    internal void Update(Idas3OrnamentMotion motion,float alpha=1)
    {
        for(int joint=0;joint<joints.Length;++joint){
            var restJoint=joints[joint];
            if(joint<=fixedJoint){
                posedJoints[joint]=restJoint;rotations[joint]=Quaternion.identity;
                transforms[joint]=Matrix4x4.identity;continue;
            }
            // Original ChainJoint0..7 form one ordered hierarchy. Each pivot
            // belongs to its parent's rigid link, not an independently sampled
            // point on the simulation curve. FK retains every bind distance,
            // including the fixed first connector and its first moving pivot.
            int parent=joint-1;
            posedJoints[joint]=posedJoints[parent]+rotations[parent]*(restJoint-joints[parent]);
            int child=Mathf.Min(joint+1,joints.Length-1);
            float startDepth=-restJoint.y;
            float endDepth=child!=joint?-joints[child].y:Idas3OrnamentMotion.ChainLength;
            var tangent=CurvePoint(motion,endDepth,alpha)-CurvePoint(motion,startDepth,alpha);
            rotations[joint]=tangent.sqrMagnitude>1e-10f
                ?Quaternion.FromToRotation(Vector3.down,tangent.normalized):rotations[parent];
            transforms[joint]=Matrix4x4.TRS(posedJoints[joint],rotations[joint],Vector3.one)*Matrix4x4.Translate(-restJoint);
        }
        // The pendant follows the actual last skinned metal ring. The physics
        // endpoint can differ slightly after preserving recovered link lengths.
        AttachmentPosition=transforms[attachmentJoint].MultiplyPoint3x4(RestAttachment);
        for(int vertex=0;vertex<rest.Length;++vertex){
            var binding=weights[vertex];var position=Vector3.zero;var normal=Vector3.zero;
            for(int influence=0;influence<binding.weights.Length;++influence){
                float weight=binding.weights[influence];if(weight<=0)continue;
                var transform=transforms[binding.joints[influence]];
                position+=transform.MultiplyPoint3x4(rest[vertex])*weight;
                normal+=transform.MultiplyVector(normals[vertex])*weight;
            }
            vertices[vertex]=position;deformedNormals[vertex]=normal.normalized;
        }
        mesh.vertices=vertices;mesh.normals=deformedNormals;deformed=true;
    }

    Vector3 CurvePoint(Idas3OrnamentMotion motion,float depth,float alpha)
    {
        // The fixed source joint also owns the first metal connector. Curve
        // node zero therefore drives rotation at the first MOVABLE pivot.
        // Starting at the fixed source joint would discard the upper bend.
        float flexibleStart=-joints[fixedJoint+1].y;
        float sample=(depth-flexibleStart)
            /(Idas3OrnamentMotion.ChainLength-flexibleStart)
            *(Idas3OrnamentMotion.ChainNodeCount-1);
        sample=Mathf.Clamp(sample,0,Idas3OrnamentMotion.ChainNodeCount-1);
        int segment=Mathf.Min(Mathf.FloorToInt(sample),Idas3OrnamentMotion.ChainNodeCount-2);
        return Vector3.Lerp(motion.RenderChainPoint(segment,alpha),motion.RenderChainPoint(segment+1,alpha),sample-segment);
    }
}

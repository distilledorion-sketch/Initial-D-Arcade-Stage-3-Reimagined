using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;

// Opt-in, private-save player diagnostic. Never invoked by ordinary gameplay.
public sealed partial class Idas8HakoneCourse
{
    struct SceneryState {
        public int lod; public bool enabled; public Mesh mesh; public Material material;
        public Quaternion rotation;
    }
    SceneryState[] SaveScenery() {
        var result=new SceneryState[scenery.Count];
        for(int i=0;i<result.Length;i++){var s=scenery[i];result[i]=new SceneryState{
            lod=s.lod,enabled=s.renderer.enabled,mesh=s.filter.sharedMesh,
            material=s.renderer.sharedMaterial,rotation=s.transform.rotation};}
        return result;
    }
    void RestoreScenery(SceneryState[] saved) {
        for(int i=0;i<saved.Length;i++){var s=scenery[i];var old=saved[i];
            s.lod=old.lod;s.renderer.enabled=old.enabled;s.filter.sharedMesh=old.mesh;
            s.renderer.sharedMaterial=old.material;s.transform.rotation=old.rotation;}
    }
    // Previous updater semantics: Unity Transform reads inside the loop,
    // repeated culled-renderer writes, and authored near-tree rotation each frame.
    void UpdateSceneryReference() {
        foreach(var item in scenery){
            float d=(item.filter.transform.position-view.transform.position).sqrMagnitude;
            bool tree=item.source.kind=="tree";
            int lod=tree?TreeLod(d,item.lod):(d<500*500?0:-1);
            if(lod<0){item.renderer.enabled=false;item.lod=lod;continue;}
            if(item.lod!=lod){item.lod=lod;item.renderer.enabled=true;
                int index=item.source.meshes[lod];item.filter.sharedMesh=sourceMeshes[index];
                item.renderer.sharedMaterial=materials[sourceMaterials[index]];}
            if(tree&&lod!=2)item.filter.transform.rotation=item.authoredRotation;
            else {Vector3 facing=view.transform.position-item.filter.transform.position;facing.y=0;
                if(facing.sqrMagnitude>.01f)item.filter.transform.rotation=Quaternion.LookRotation(facing,Vector3.up);}
        }
    }
    double MeasureScenery(bool baseline,Vector3[] probes,int repeats) {
        var timer=Stopwatch.StartNew();
        for(int r=0;r<repeats;r++)foreach(var p in probes){view.transform.position=p;
            if(baseline)UpdateSceneryReference();else UpdateScenery(view.transform.position);}
        timer.Stop();return timer.Elapsed.TotalMilliseconds/(repeats*probes.Length);
    }
    public void CheckSceneryPerformance(string output,string label,bool benchmark) {
        var saved=SaveScenery();var cameraPosition=view.transform.position;
        var probes=new List<Vector3>();
        for(int i=0;i<roads[0].Length;i+=Math.Max(1,roads[0].Length/64))probes.Add(roads[0][i]+Vector3.up*2);
        int forward=probes.Count;
        for(int i=forward-1;i>=0;i--)probes.Add(probes[i]);
        // Cross every LOD hysteresis/culling boundary in both directions.
        float[] distances={0,233.99f,234.01f,259.99f,260.01f,285.99f,286.01f,499.99f,500.01f,
            539.99f,540.01f,599.99f,600.01f,659.99f,660.01f,1439.99f,1440.01f,1599.99f,1600.01f,1759.99f,1760.01f};
        for(int i=0;i<scenery.Count;i+=Math.Max(1,scenery.Count/8)){
            foreach(float d in distances)probes.Add(scenery[i].position+Vector3.right*d);
            for(int j=distances.Length-1;j>=0;j--)probes.Add(scenery[i].position+Vector3.right*distances[j]);
        }
        long comparisons=0;
        try {
            // Include a fresh placement state, then preserve state across all probes.
            foreach(var s in scenery){s.lod=-2;s.transform.rotation=s.authoredRotation;}
            foreach(var p in probes){
                view.transform.position=p;var before=SaveScenery();UpdateSceneryReference();var expected=SaveScenery();
                RestoreScenery(before);UpdateScenery(view.transform.position);
                for(int i=0;i<scenery.Count;i++){
                    var s=scenery[i];var e=expected[i];var q=s.transform.rotation;
                    if(s.position!=s.transform.position||s.tree!=(s.source.kind=="tree")||
                        s.lod!=e.lod||s.renderer.enabled!=e.enabled||s.filter.sharedMesh!=e.mesh||s.renderer.sharedMaterial!=e.material||
                        Math.Abs(q.x-e.rotation.x)>1e-6f||Math.Abs(q.y-e.rotation.y)>1e-6f||
                        Math.Abs(q.z-e.rotation.z)>1e-6f||Math.Abs(q.w-e.rotation.w)>1e-6f)
                        throw new InvalidOperationException("Scenery mismatch "+label+" item "+i+" camera "+p);
                    comparisons++;
                }
            }
            File.AppendAllText(Path.Combine(output,"scenery-equivalence.csv"),$"{label},{scenery.Count},{probes.Count},{comparisons},PASS\n");
            if(benchmark){
                var trajectory=new Vector3[forward];probes.CopyTo(0,trajectory,0,forward);
                RestoreScenery(saved);MeasureScenery(true,trajectory,2);
                RestoreScenery(saved);MeasureScenery(false,trajectory,2);
                // ABBA order, identical starting states and camera routes. No captures/logging in timed loops.
                for(int round=0;round<3;round++)foreach(bool baseline in new[]{true,false,false,true}){
                    RestoreScenery(saved);GC.Collect();
                    double ms=MeasureScenery(baseline,trajectory,8);
                    File.AppendAllText(Path.Combine(output,"scenery-timing.csv"),$"{label},{round},{(baseline?"baseline":"optimized")},{ms.ToString("R",System.Globalization.CultureInfo.InvariantCulture)}\n");
                }
            }
        } finally {RestoreScenery(saved);view.transform.position=cameraPosition;}
    }
}

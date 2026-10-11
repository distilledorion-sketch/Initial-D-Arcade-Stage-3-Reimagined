using System;
using System.IO;
using System.Linq;
using System.Reflection;
using UnityEditor;
using UnityEngine;

// Isolated production-loader captures. Does not initialize the game or saves.
public static class Idas3ImportedLampChecks
{
    const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public;
    static void Set(object o,string name,object value)=>o.GetType().GetField(name,Hidden).SetValue(o,value);
    static object Get(object o,string name)=>o.GetType().GetField(name,Hidden).GetValue(o);
    static object Call(object o,string name,params object[] args)=>o.GetType().GetMethod(name,Hidden).Invoke(o,args);
    static Vector3 Vec(BinaryReader r)=>new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    static string output;
    public static void RunAndRebuildPlayer(){
        Run();
        // Presentation-only rebuild into the previously staged, verified pack.
        // No course data changed, so retain its existing StreamingAssets.
        const string player="Builds/Current/InitialDUnity.exe";
        if(!File.Exists(player))throw new FileNotFoundException("Stage a full player first",player);
        foreach(string relative in new[]{"GUNSAI/night_dry/lamp-glows.json","ODAWARA/night_dry/lamp-glows.json","ODAWARA/night_dry/lamp-glare.dds"}){
            if(!File.ReadAllBytes("RuntimeAssets/"+relative).SequenceEqual(File.ReadAllBytes("Builds/Current/InitialDUnity_Data/StreamingAssets/"+relative)))
                throw new Exception("Restage changed runtime asset: "+relative);
        }
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=player,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.CompressWithLz4HC});
        if(report.summary.result!=UnityEditor.Build.Reporting.BuildResult.Succeeded)throw new Exception("Lamp player build failed");
    }
    public static void Run(){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-lamp-check-label");
        output=Path.GetFullPath("Verification/idzero-lamps-20261009/"+(at>=0?args[at+1]:"current"));
        Directory.CreateDirectory(output);ShaderUtil.allowAsyncCompilation=false;
        CheckAuthoredNormalStability();
        foreach(string name in new[]{"ODAWARA","GUNSAI"})foreach(bool night in new[]{true,false})foreach(bool wet in new[]{false,true})Capture(name,night,wet);
        foreach(string name in new[]{"Idas3Scene","Idas3SceneDirect","Idas3ImportedLampGlow"}){
            var shader=Resources.Load<Shader>(name);if(shader==null){if(name.EndsWith("Glow"))continue;throw new Exception(name);}
            if(!shader.isSupported)throw new Exception(name+" unsupported");
            foreach(var e in ShaderUtil.GetShaderMessages(shader))if(e.severity.ToString()=="Error")throw new Exception(e.message);
        }
        File.WriteAllText(Path.Combine(output,"PASS.txt"),"Production imported course loaders and shaders rendered day/night, dry/wet, both camera directions. Authored slope normals, camera-independent stationary lighting, complete-lamp reference, spatial cell coverage, road illumination, glow draw count, mirror frame selection and occlusion checks passed.\n");
        Debug.Log("IMPORTED_LAMP_CHECKS_PASS");
    }
    static void Capture(string name,bool night,bool wet){
        var go=new GameObject("Private lamp camera");var camera=go.AddComponent<Camera>();camera.enabled=false;
        camera.fieldOfView=60;camera.aspect=16f/9;camera.nearClipPlane=.3f;camera.farClipPlane=12000;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;camera.cullingMask=1<<28;
        var host=go.AddComponent<Idas3SceneGame>();host.enabled=false;
        var property=typeof(Idas3SceneGame).GetProperty("Status",Hidden);var status=Activator.CreateInstance(property.PropertyType);
        status.GetType().GetField("flags").SetValue(status,16384u|(name=="GUNSAI"?33554432u:67108864u)|(night?65536u:0u)|(wet?131072u:0u));property.SetValue(host,status);
        var group=new GameObject("Private imported lamps");var course=group.AddComponent<Idas8HakoneCourse>();course.enabled=false;
        string pack=Path.GetFullPath(Path.Combine("RuntimeAssets",name)),root=Path.Combine(pack,night?"night_dry":"");
        Set(course,"host",host);Set(course,"view",camera);Set(course,"root",root);Set(course,"<LoadedCourse>k__BackingField",name);
        Set(course,"data",JsonUtility.FromJson<Idas8HakoneCourse.Manifest>(File.ReadAllText(Path.Combine(root,"scene.json"))));
        if(wet)Set(course,"wet",JsonUtility.FromJson<Idas8HakoneCourse.WetLook>(File.ReadAllText(Path.Combine(root,"wet.json"))));
        using(var buffer=new ComputeBuffer(46,16)){
            Call(course,"LoadRoad");Call(course,"LoadScene");Call(course,"UpdateDirection");Call(course,"UpdateFoliageAntialiasing");
            var glow=group.GetComponentsInChildren<MeshRenderer>().SingleOrDefault(r=>r.name=="Night lamp coronas");
            if((glow!=null)!=(name=="ODAWARA"&&night))throw new Exception("Wrong day/night glow count");
            if(glow!=null&&glow.GetComponent<MeshFilter>().sharedMesh.vertexCount!=4380)throw new Exception("Missing lens geometry");
            var roads=(Vector3[][])Get(course,"roads");Vector3[] lamps;
            using(var r=new BinaryReader(File.OpenRead(Path.Combine(pack,"lamps.bin")))){r.ReadBytes(4);lamps=new Vector3[r.ReadInt32()];for(int i=0;i<lamps.Length;i++)lamps[i]=Vec(r);}
            var courseMaterials=(Material[])Get(course,"materials");
            if(night&&!wet)CheckGridCoverage(course,name);
            foreach(bool reverse in new[]{false,true})foreach(int p in name=="ODAWARA"?new[]{1856,1066,100,0}:new[]{120,1985,3060}){
                Set(course,"reverse",reverse);Call(course,"UpdateDirection");
                camera.transform.position=roads[0][p]+Vector3.up*1.5f;camera.transform.LookAt(roads[0][(p+(reverse?-16:16)+roads[0].Length)%roads[0].Length]+Vector3.up*1.5f);
                Call(course,"UpdateLighting",(float)p);Call(course,"UpdateScenery",camera.transform.position);
                foreach(var t in group.GetComponentsInChildren<Transform>()){t.gameObject.layer=28;var r=t.GetComponent<MeshRenderer>();if(r&&r.sharedMaterial&&r.sharedMaterial.HasProperty("_ImportedSky")&&r.sharedMaterial.GetFloat("_ImportedSky")!=0)t.position=camera.transform.position;}
                var vp=GL.GetGPUProjectionMatrix(camera.projectionMatrix,false)*camera.worldToCameraMatrix;
                if(SystemInfo.usesReversedZBuffer)vp.SetRow(2,vp.GetRow(3)-vp.GetRow(2));vp=vp.transpose;
                var words=new Vector4[46];for(int row=0;row<4;row++)words[row]=vp.GetRow(row);
                var eye=camera.transform.position;words[4]=new Vector4(eye.x,eye.y,eye.z,1);words[5]=new Vector4(.025f,.035f,.06f,night?1:0);
                var right=camera.transform.right;var up=camera.transform.up;words[8]=new Vector4(right.x,right.y,right.z,0);words[9]=new Vector4(up.x,up.y,up.z,0);
                int l=0;if(night)foreach(var lamp in lamps.OrderBy(v=>(v-eye).sqrMagnitude).Take(8))words[14+l++]=new Vector4(lamp.x,lamp.y,lamp.z,1f/(26*26));
                buffer.SetData(words);Shader.SetGlobalBuffer("_IdasFrameWords",buffer);Shader.SetGlobalInt("_IdasView",0);Shader.SetGlobalVector("_IdasDepthProjection",Vector4.zero);Shader.SetGlobalFloat("_IdasTrackLighting",1);
                string label=name+"-"+(night?"night":"day")+"-"+p+(wet?"-wet":"")+(reverse?"-reverse":"");
                var main=Shot(camera,label);
                if(name=="ODAWARA"&&night&&p==1856&&!reverse&&!wet){
                    // Keep the camera and geometry fixed, but supply the eight
                    // lights selected at another position along the course.
                    var other=(Vector4[])words.Clone();int index=0;
                    foreach(var lamp in lamps.OrderBy(v=>(v-roads[0][100]).sqrMagnitude).Take(8))
                        other[14+index++]=new Vector4(lamp.x,lamp.y,lamp.z,1f/(26*26));
                    buffer.SetData(other);var changed=Shot(camera,label+"-other-camera-lamps");buffer.SetData(words);
                    int different=main.Where((pixel,i)=>!pixel.Equals(changed[i])).Count();
                    File.WriteAllText(Path.Combine(output,"camera-light-selection.txt"),"Changed pixels: "+different+"\n");
                    if(different!=0)throw new Exception("Stationary road lighting depends on the camera's nearest lamps");
                    CheckAllLampReference(course,camera,main,label,lamps);
                }
                if(name=="ODAWARA"&&p==1856&&!reverse){
                    // Keep the visible bulbs enabled; they cannot substitute for
                    // the light cast onto the road. Disable only frame light data.
                    var unlit=(Vector4[])words.Clone();Array.Clear(unlit,14,8);buffer.SetData(unlit);
                    var grid=courseMaterials[0].GetVector("_ImportedLampGrid");
                    foreach(var m in courseMaterials)m.SetVector("_ImportedLampGrid",Vector4.zero);
                    var noLights=Shot(camera,label+"-no-road-lights");buffer.SetData(words);
                    foreach(var m in courseMaterials)m.SetVector("_ImportedLampGrid",grid);
                    int brighter=0;long gain=0;
                    // Lower central rectangle is asphalt, below the fixture.
                    for(int y=60;y<310;y++)for(int x=400;x<880;x++){
                        int index=y*1280+x;
                        int delta=main[index].r+main[index].g+main[index].b-noLights[index].r-noLights[index].g-noLights[index].b;
                        if(delta>=3)brighter++;gain+=delta;
                    }
                    File.WriteAllText(Path.Combine(output,label+"-illumination.txt"),"Brighter road pixels: "+brighter+"\nTotal RGB gain: "+gain+"\n");
                    if(night&&(brighter<1000||gain<6000))throw new Exception("Streetlamps do not illuminate the road: "+label);
                    if(!night&&!main.SequenceEqual(noLights))throw new Exception("Night lamps changed daytime scenery");
                }
                if(glow!=null&&p==1856&&!reverse&&!wet){
                    // Only the mirror slot is valid: using the main slot for a
                    // glow would now drop/misplace it while the scenery matches.
                    Array.Copy(words,0,words,23,23);Array.Clear(words,0,23);buffer.SetData(words);Shader.SetGlobalInt("_IdasView",1);
                    var mirror=Shot(camera,label+"-mirror-slot");
                    if(!main.SequenceEqual(mirror))throw new Exception("Mirror constants differ");
                    Array.Copy(words,23,words,0,23);buffer.SetData(words);Shader.SetGlobalInt("_IdasView",0);
                    // A nearby opaque wall must hide both the lens and corona.
                    var block=GameObject.CreatePrimitive(PrimitiveType.Cube);block.layer=28;
                    var material=new Material(Shader.Find("Unlit/Color")){color=Color.black};block.GetComponent<MeshRenderer>().sharedMaterial=material;
                    block.transform.position=Vector3.Lerp(camera.transform.position,lamps[0],.8f);block.transform.rotation=camera.transform.rotation;block.transform.localScale=new Vector3(5,5,.3f);
                    var visible=Shot(camera,label+"-occluded");glow.enabled=false;var hidden=Shot(camera,label+"-occluded-no-glow");glow.enabled=true;
                    // Project directly to capture pixels, independent of editor GameView dimensions.
                    var v=camera.WorldToViewportPoint(lamps[0]);int x=(int)(v.x*1280),y=(int)(v.y*720);
                    for(int dy=-18;dy<=18;dy++)for(int dx=-18;dx<=18;dx++){int pixel=(y+dy)*1280+x+dx;if(!visible[pixel].Equals(hidden[pixel]))throw new Exception("Glow visible through blocker");}
                    block.SetActive(false);UnityEngine.Object.DestroyImmediate(block);UnityEngine.Object.DestroyImmediate(material);
                }
            }
        }
        group.SetActive(false);go.SetActive(false);
        Call(course,"ClearLampLightingGrid");
    }
    static void CheckGridCoverage(Idas8HakoneCourse course,string name){
        var grid=Get(course,"lampLightingGrid");if(grid==null)throw new Exception("Missing stationary light grid");
        var bounds=(Vector4)Get(grid,"bounds");var cells=(Vector2Int[])Get(grid,"cells");
        var entries=(Vector4[])Get(grid,"lights");var lamps=(Vector3[])Get(grid,"source");
        int width=(int)bounds.z,max=0,occupied=0,total=0;
        // Independent circle/rectangle intersection verifies the entire grid,
        // including cell edges. No spatially relevant lamp may be discarded.
        for(int i=0;i<cells.Length;i++){
            var span=cells[i];if(span.x<0||span.y<0||span.x+span.y>entries.Length)throw new Exception("Invalid light span");
            max=Math.Max(max,span.y);if(span.y>0){occupied++;total+=span.y;}
            float x=bounds.x+(i%width)*32,z=bounds.y+(i/width)*32;
            foreach(var lamp in lamps){
                float dx=lamp.x-Mathf.Clamp(lamp.x,x,x+32),dz=lamp.z-Mathf.Clamp(lamp.z,z,z+32);
                if(dx*dx+dz*dz>=26*26)continue;
                bool found=false;
                for(int j=span.x;j<span.x+span.y;j++)if((Vector3)entries[j]==lamp){found=true;break;}
                if(!found)throw new Exception("Light influence clipped at a grid boundary");
            }
        }
        File.WriteAllText(Path.Combine(output,name+"-grid.txt"),$"Lamps: {lamps.Length}\nCells: {cells.Length}\nReferences: {entries.Length}\nMaximum lamps per cell: {max}\nMean per occupied cell: {(float)total/occupied}\nGPU bytes: {cells.Length*8+entries.Length*16}\n");
    }
    public static void RunAuthoredNormalCheck(){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-lamp-check-label");
        output=Path.GetFullPath("Verification/idzero-lamps-20261009/"+(at>=0?args[at+1]:"normal-check"));
        Directory.CreateDirectory(output);ShaderUtil.allowAsyncCompilation=false;CheckAuthoredNormalStability();
    }
    static void CheckAuthoredNormalStability(){
        // Imported uphill meshes can carry artist-authored upward lighting
        // normals. Moving the eye above/below them must not invert those normals.
        var go=new GameObject("Private sloped road camera");var camera=go.AddComponent<Camera>();camera.enabled=false;
        camera.transform.position=new Vector3(0,6,-22);camera.transform.LookAt(new Vector3(0,6,0));
        camera.fieldOfView=60;camera.aspect=16f/9;camera.nearClipPlane=.3f;camera.farClipPlane=100;
        camera.cullingMask=1<<27;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;
        var road=new GameObject("Authored sloped road");road.layer=27;
        var mesh=new Mesh{vertices=new[]{new Vector3(-8,0,-8),new Vector3(8,0,-8),new Vector3(-8,12,8),new Vector3(8,12,8)},
            normals=Enumerable.Repeat(Vector3.up,4).ToArray(),colors=Enumerable.Repeat(Color.black,4).ToArray(),
            uv=new Vector2[4],uv2=new Vector2[4],uv3=new Vector2[4],triangles=new[]{0,2,1,1,2,3}};
        var material=new Material(Resources.Load<Shader>("Idas3SceneDirect"));material.EnableKeyword("IDAS_IMPORTED_COURSE");
        material.SetFloat("_ImportedNight",1);material.SetFloat("_ImportedAuthoredNormals",1);
        road.AddComponent<MeshFilter>().sharedMesh=mesh;road.AddComponent<MeshRenderer>().sharedMaterial=material;
        using(var buffer=new ComputeBuffer(46,16))try{
            var vp=GL.GetGPUProjectionMatrix(camera.projectionMatrix,false)*camera.worldToCameraMatrix;
            if(SystemInfo.usesReversedZBuffer)vp.SetRow(2,vp.GetRow(3)-vp.GetRow(2));vp=vp.transpose;
            var words=new Vector4[46];for(int i=0;i<4;i++)words[i]=vp.GetRow(i);
            words[4]=new Vector4(0,1,-22,1);words[5]=new Vector4(.025f,.035f,.06f,1);
            words[14]=new Vector4(0,16,0,1f/(26*26));buffer.SetData(words);
            Shader.SetGlobalBuffer("_IdasFrameWords",buffer);Shader.SetGlobalInt("_IdasView",0);Shader.SetGlobalVector("_IdasDepthProjection",Vector4.zero);
            var low=Shot(camera,"authored-normal-low-eye");words[4].y=20;buffer.SetData(words);
            var high=Shot(camera,"authored-normal-high-eye");
            if(low.Count(p=>p.r>0)<10000)throw new Exception("Sloped road fixture did not render");
            int different=low.Where((p,i)=>!p.Equals(high[i])).Count();
            File.WriteAllText(Path.Combine(output,"authored-normal-stability.txt"),"Changed pixels: "+different+"\n");
            if(different!=0)throw new Exception("Authored uphill lighting flips when the surface crosses eye height");
        }finally{
            UnityEngine.Object.DestroyImmediate(road);UnityEngine.Object.DestroyImmediate(mesh);UnityEngine.Object.DestroyImmediate(material);UnityEngine.Object.DestroyImmediate(go);
        }
    }
    static void CheckAllLampReference(Idas8HakoneCourse course,Camera camera,Color32[] main,string label,Vector3[] lamps){
        var grid=Get(course,"lampLightingGrid");var cells=(Vector2Int[])Get(grid,"cells");
        var materials=(Material[])Get(course,"materials");
        using(var allCells=new ComputeBuffer(cells.Length,8))using(var allLights=new ComputeBuffer(lamps.Length,16)){
            allCells.SetData(Enumerable.Repeat(new Vector2Int(0,lamps.Length),cells.Length).ToArray());
            allLights.SetData(lamps.Select(p=>new Vector4(p.x,p.y,p.z,1f/(26*26))).ToArray());
            try{
                foreach(var m in materials){m.SetBuffer("_ImportedLampCells",allCells);m.SetBuffer("_ImportedLampLights",allLights);}
                var reference=Shot(camera,label+"-all-lamps-reference");int maxDelta=0;
                for(int i=0;i<main.Length;i++)maxDelta=Math.Max(maxDelta,Math.Max(Math.Abs(main[i].r-reference[i].r),Math.Max(Math.Abs(main[i].g-reference[i].g),Math.Abs(main[i].b-reference[i].b))));
                File.WriteAllText(Path.Combine(output,"all-lamps-reference.txt"),"Maximum channel error: "+maxDelta+"\n");
                if(maxDelta>1)throw new Exception("Light grid differs from the complete lamp set");
            }finally{
                foreach(var m in materials){m.SetBuffer("_ImportedLampCells",(ComputeBuffer)Get(course,"lampCellBuffer"));m.SetBuffer("_ImportedLampLights",(ComputeBuffer)Get(course,"lampLightBuffer"));}
            }
        }
    }
    static Color32[] Shot(Camera camera,string name){
        var target=new RenderTexture(1280,720,24){antiAliasing=4};target.Create();
        var old=RenderTexture.active;var image=new Texture2D(1280,720,TextureFormat.RGB24,false);
        try{camera.targetTexture=target;camera.Render();camera.Render();RenderTexture.active=target;image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();File.WriteAllBytes(Path.Combine(output,name+".png"),image.EncodeToPNG());return image.GetPixels32();}
        finally{camera.targetTexture=null;RenderTexture.active=old;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(image);}
    }
}

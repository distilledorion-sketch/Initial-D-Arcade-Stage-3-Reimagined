using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Profiling;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private bool OnlinePreloadCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-online-preload-check")>=0;
        private int preloadHeldKey,preloadCycleCount;
        private readonly List<PreloadObservation> preloadObservations=new List<PreloadObservation>();
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3SceneModeFlowValue(int field);
        [Serializable] private class PreloadObservation {
            public string name,course,importCourse,importVariant,session;
            public uint flags;
            public int importMeshes,importMaterials,importTextures,specialMeshes,specialMaterials,specialTextures;
            public int unityMeshes,unityMaterials,unityTextures,sceneTextures,uiTextures;
            public long importBytes,specialBytes,unityAllocated,monoUsed;
        }
        [Serializable] private class PreloadReport {
            public bool passed;public string role;public int cycles,onlineCourse;
            public string scope="Private offline course preloading through normal menus/pause commands, then a real two-client local TCP room and original-course race. Manual hidden rendering; not Internet EOS or foreground FPS.";
            public PreloadObservation[] observations;
        }
        private PreloadObservation ObservePreloadResources(string name)
        {
            var imported=FindAnyObjectByType<Idas8HakoneCourse>();var special=FindAnyObjectByType<IdasSpecialStageEnnaCourse>();
            var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
            var row=new PreloadObservation{name=name,course=Idas3CourseCatalog.SceneName(host.Status),flags=host.Status.flags,session=session.StateName,
                importCourse=imported.LoadedCourse,importVariant=imported.LoadedVariant,
                importMeshes=imported.RetainedMeshCount,importMaterials=imported.RetainedMaterialCount,importTextures=imported.RetainedTextureCount,
                specialMeshes=special.RetainedMeshCount,specialMaterials=special.RetainedMaterialCount,specialTextures=special.RetainedTextureCount,
                importBytes=imported.RetainedResourceBytes(),specialBytes=special.RetainedResourceBytes(),
                unityMeshes=Resources.FindObjectsOfTypeAll<Mesh>().Length,unityMaterials=Resources.FindObjectsOfTypeAll<Material>().Length,
                unityTextures=Resources.FindObjectsOfTypeAll<Texture2D>().Length,sceneTextures=scene.UploadedTextureCount,uiTextures=ui.SourceTextureCount,
                unityAllocated=Profiler.GetTotalAllocatedMemoryLong(),monoUsed=Profiler.GetMonoUsedSizeLong()};
            preloadObservations.Add(row);
            File.WriteAllText(Path.Combine(root,"preload-observations.json"),JsonUtility.ToJson(new PreloadReport{
                role=role,cycles=preloadCycleCount,onlineCourse=course,observations=preloadObservations.ToArray()},true));
            return row;
        }
        private IEnumerator PreloadKey(int key)
        {
            preloadHeldKey=key;yield return new WaitForSecondsRealtime(.10f);
            preloadHeldKey=0;yield return new WaitForSecondsRealtime(.12f);
        }
        private IEnumerator PreloadReturnToCourse()
        {
            preloadHeldKey=0;
            var pause=host.GetComponent<Idas3PauseMenu>();pause.SetOpen(true);yield return Frames(3);
            Check(pause.IsOpen,"Preload could not open the ordinary race pause menu.");
            pause.Navigate(1);pause.Navigate(1);pause.Navigate(1);pause.Activate();
            pause.NavigateHorizontal(1);pause.Activate();
            yield return Until(()=>!pause.IsOpen&&(host.Status.flags&1u)!=0&&host.Status.frontendStage==6,10,"Preload return command did not reach Course6.");
            yield return Frames(3);
        }
        private IEnumerator PreloadOfflineCourses()
        {
            Check(!onlineCheck&&!P2pRaceCheck&&!ChallengerCheck&&peerRoot.Length>0&&course<9,"Preload checks require isolated local peers and an original online course.");
            preloadCycleCount=IntegerArgument(Environment.GetCommandLineArgs(),"-idas3-online-preload-cycles",2,0,3);
            ObservePreloadResources("before-preloading");
            if(preloadCycleCount>0){
                Phase("offline-preload-start");yield return PreloadKey(13);yield return PreloadKey(116);
                yield return Until(()=>(host.Status.flags&1u)==0&&host.Status.racePhase==2,35,"Preload setup race did not start.");
                for(int cycle=0;cycle<preloadCycleCount;++cycle)foreach(int selected in new[]{16,17}){
                    Phase("offline-preload-"+cycle+"-"+selected);yield return PreloadReturnToCourse();
                    for(int move=0;Idas3SceneModeFlowValue(30)!=selected&&move<Idas3CourseCatalog.Count;++move)yield return PreloadKey(39);
                    Check(Idas3SceneModeFlowValue(30)==selected,"Preload carousel selected the wrong course.");
                    bool nightWet=(cycle&1)!=0;
                    for(int choice=0;(host.Status.flags&1u)!=0&&choice<8;++choice){
                        int stage=host.Status.frontendStage,conditions=Idas3SceneModeFlowValue(31);
                        Check(stage>=6&&stage<=9,"Preload left the course/route/conditions menus.");
                        if((stage==8&&nightWet!=((conditions&1)!=0))||(stage==9&&nightWet!=((conditions&2)!=0)))yield return PreloadKey(39);
                        yield return PreloadKey(13);
                        yield return new WaitForSecondsRealtime(.8f);
                    }
                    yield return Until(()=>(host.Status.flags&1u)==0&&host.Status.racePhase==2,35,"Preloaded course did not reach driving.");
                    yield return Frames(3);
                    var row=ObservePreloadResources("preload-"+cycle+"-"+selected);
                    Check(row.importCourse==(selected==16?"GUNSAI":"ODAWARA")&&row.importVariant==(nightWet?"night_wet":"day_dry")&&row.importMeshes>0,
                        "Preload did not construct the requested course and variant.");
                }
                yield return PreloadReturnToCourse();
            }
            ObservePreloadResources("pre-online-menu");
            File.WriteAllText(Path.Combine(root,"preloading-finished.txt"),"Offline preload sequence completed.");
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"preloading-finished.txt")),120,"Peer did not complete its offline preloads.");
        }
        private IEnumerator VerifyOnlineAfterPreload()
        {
            Phase("online-after-preload");
            var row=ObservePreloadResources("online-original-start");
            Check(row.course==Idas3CourseCatalog.Names[course]&&row.importMeshes==0&&row.importMaterials==0&&row.importTextures==0&&
                row.specialMeshes==0&&row.specialMaterials==0&&row.specialTextures==0&&row.importBytes==0&&row.specialBytes==0,
                "Online original race retained imported scenery resources.");
            var camera=host.GetComponent<Camera>();var previous=camera.targetTexture;
            int previousAa=QualitySettings.antiAliasing;
            var output=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Online preload diagnostic",antiAliasing=4};
            Idas3TransitionProfile profile=null;
            try {
                Check(output.Create(),"Online preload render target creation failed.");
                QualitySettings.antiAliasing=4;camera.targetTexture=output;yield return Frames(4);
                profile=host.gameObject.AddComponent<Idas3TransitionProfile>();profile.Initialize(host,output,root);
                profile.Section("online-original-driving");
                yield return new WaitForSecondsRealtime(6f);
                Check(session.IsRacing&&session.RaceReleased&&!session.DisconnectedFinish&&localTravel>10&&remoteTravel>10,
                    "Online race stopped or failed to move during the preload comparison.");
                profile.Checkpoint("online-original");profile.StopAndSave(true,null);
                Check(profile.Failure==null,profile.Failure);
            }finally{
                if(profile!=null){profile.StopAndSave(false,"Diagnostic cleanup after an incomplete sample.");profile.RestoreCameras();Destroy(profile);}
                camera.targetTexture=previous;QualitySettings.antiAliasing=previousAa;
                host.GetComponent<Idas3SceneRenderer>().ApplyFrame();host.GetComponent<Idas3UnityUi>().ApplyFrame();
                output.Release();Destroy(output);
            }
            ObservePreloadResources("online-original-end");
            File.WriteAllText(Path.Combine(root,"online-preload.json"),JsonUtility.ToJson(new PreloadReport{passed=true,role=role,
                cycles=preloadCycleCount,onlineCourse=course,observations=preloadObservations.ToArray()},true));
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"online-preload.json")),12,"Peer did not finish the online preload comparison.");
            driving=false;session.LeaveRoom();yield return Frames(8);
            Check(!session.IsRacing&&!session.InLobby,"Preload check did not leave its private room cleanly.");
        }
    }
}

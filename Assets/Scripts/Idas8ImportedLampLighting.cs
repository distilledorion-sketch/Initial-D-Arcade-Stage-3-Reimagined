using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public sealed partial class Idas8HakoneCourse
{
    // Stationary scenery must not use the camera's nearest-eight car lights.
    // Index each lamp into EVERY 32 m cell touched by its 26 m influence.
    // A fragment evaluates only its cell's lights, independent of either camera.
    const float LampCellSize=32, LampRadius=26;
    sealed class LampLightingGrid {
        public Vector4 bounds;
        public Vector2Int[] cells;
        public Vector4[] lights;
        public Vector3[] source;
    }
    LampLightingGrid lampLightingGrid;
    ComputeBuffer lampCellBuffer,lampLightBuffer;

    static LampLightingGrid BuildLampLightingGrid(Vector3[] positions){
        if(positions.Length==0||positions.Length>1024)throw new InvalidDataException("Lamp count");
        var min=positions[0];var max=min;
        foreach(var p in positions){
            if(float.IsNaN(p.sqrMagnitude)||float.IsInfinity(p.sqrMagnitude))throw new InvalidDataException("Lamp position");
            min=Vector3.Min(min,p);max=Vector3.Max(max,p);
        }
        float x0=Mathf.Floor((min.x-LampRadius)/LampCellSize)*LampCellSize;
        float z0=Mathf.Floor((min.z-LampRadius)/LampCellSize)*LampCellSize;
        int width=Mathf.FloorToInt((max.x+LampRadius-x0)/LampCellSize)+1;
        int height=Mathf.FloorToInt((max.z+LampRadius-z0)/LampCellSize)+1;
        if(width<=0||height<=0||(long)width*height>262144)throw new InvalidDataException("Lamp grid extent");
        var lists=new List<Vector4>[width*height];
        foreach(var p in positions){
            int xMin=Mathf.FloorToInt((p.x-LampRadius-x0)/LampCellSize),xMax=Mathf.FloorToInt((p.x+LampRadius-x0)/LampCellSize);
            int zMin=Mathf.FloorToInt((p.z-LampRadius-z0)/LampCellSize),zMax=Mathf.FloorToInt((p.z+LampRadius-z0)/LampCellSize);
            var light=new Vector4(p.x,p.y,p.z,1/(LampRadius*LampRadius));
            for(int z=zMin;z<=zMax;z++)for(int x=xMin;x<=xMax;x++){
                int index=z*width+x;
                if(lists[index]==null)lists[index]=new List<Vector4>();
                lists[index].Add(light);
            }
        }
        var cells=new Vector2Int[lists.Length];var lights=new List<Vector4>();
        for(int i=0;i<cells.Length;i++){
            cells[i]=new Vector2Int(lights.Count,lists[i]?.Count??0);
            if(lists[i]!=null)lights.AddRange(lists[i]);
        }
        return new LampLightingGrid{bounds=new Vector4(x0,z0,width,height),cells=cells,lights=lights.ToArray(),source=positions};
    }
    void LoadLampLightingGrid(){
        if(!IdZero(LoadedCourse)||(SceneFlags&65536u)==0)return;
        string path=Path.Combine(Path.GetDirectoryName(root),"lamps.bin");
        if(!File.Exists(path))return; // Older optional packs retain their fallback.
        Vector3[] positions;
        using(var r=new BinaryReader(File.OpenRead(path))){
            Magic(r,"HKL1");int count=r.ReadInt32();
            if(count<0||count>1024)throw new InvalidDataException("Lamp count");
            positions=new Vector3[count];for(int i=0;i<count;i++)positions[i]=Vec(r);
            if(r.BaseStream.Position!=r.BaseStream.Length)throw new InvalidDataException("Trailing lamp data");
        }
        if(positions.Length==0)return;
        lampLightingGrid=BuildLampLightingGrid(positions);
        lampCellBuffer=new ComputeBuffer(lampLightingGrid.cells.Length,8);lampCellBuffer.SetData(lampLightingGrid.cells);
        lampLightBuffer=new ComputeBuffer(lampLightingGrid.lights.Length,16);lampLightBuffer.SetData(lampLightingGrid.lights);
        foreach(var material in materials){
            material.SetVector("_ImportedLampGrid",lampLightingGrid.bounds);
            material.SetBuffer("_ImportedLampCells",lampCellBuffer);material.SetBuffer("_ImportedLampLights",lampLightBuffer);
        }
        Debug.Log($"{LoadedCourse} stationary lamps: {positions.Length}, cells {lampLightingGrid.cells.Length}, references {lampLightingGrid.lights.Length}");
    }
    void ClearLampLightingGrid(){
        lampCellBuffer?.Release();lampLightBuffer?.Release();
        lampCellBuffer=null;lampLightBuffer=null;lampLightingGrid=null;
    }
}

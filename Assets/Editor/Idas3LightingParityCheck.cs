using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;

// Isolated shader test. No native game instance, controller, audio or saves.
public static class Idas3LightingParityCheck
{
    [StructLayout(LayoutKind.Sequential)]
    struct UInt4 { public uint x,y,z,w; }
    [Serializable]
    sealed class Report {
        public bool passed;
        public int cases, comparisons, failures;
        public float maximumError, tolerance = 0.00002f;
        public string device, graphicsApi, colorSpace, scope, error;
    }
    static Vector4 ReadVector(BinaryReader r) => new Vector4(r.ReadSingle(),r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    public static void Run()
    {
        var folder=Path.GetFullPath("Verification/lighting-gpu-20260918");
        Directory.CreateDirectory(folder);
        var report=new Report { device=SystemInfo.graphicsDeviceName,
            graphicsApi=SystemInfo.graphicsDeviceType.ToString(), colorSpace=QualitySettings.activeColorSpace.ToString(),
            scope="Unity-compiled production courseColors versus unchanged Flycast primary HLSL colors. Actual 36 course condition rows, eight view transforms and 16 normals per row; routing, attenuation, overflow, material flags and bypass corners. Both view indices and all four ARRAY slots. This tests lighting math/buffer indexing, not rasterization, geometry, camera ownership or cabinet calibration." };
        try {
            var shader=AssetDatabase.LoadAssetAtPath<ComputeShader>("Assets/Editor/Idas3LightingParity.compute");
            if(shader==null||!SystemInfo.supportsComputeShaders)throw new Exception("Lighting compute shader unavailable");
            int kernel=shader.FindKernel("CompareLighting");
            using(var lights=new ComputeBuffer(312,16,ComputeBufferType.Structured))
            using(var output=new ComputeBuffer(2,16,ComputeBufferType.Structured))
            using(var reader=new BinaryReader(File.OpenRead(Path.Combine(folder,"fixtures.bin"))))
            using(var mismatch=new StreamWriter(Path.Combine(folder,"mismatches.csv"))) {
                if(reader.ReadUInt32()!=0x3147544c)throw new Exception("Wrong lighting fixture magic");
                int count=reader.ReadInt32();if(count<=0||count>10000)throw new Exception("Invalid lighting fixture count");
                var words=new UInt4[312];var raw=new UInt4[39];var actual=new Vector4[2];
                shader.SetBuffer(kernel,"_IdasLightWords",lights);shader.SetBuffer(kernel,"_FixtureResult",output);
                mismatch.WriteLine("case,view,scope,color,channel,expected,actual,error");
                for(int test=0;test<count;++test) {
                    for(int i=0;i<raw.Length;++i)raw[i]=new UInt4{x=reader.ReadUInt32(),y=reader.ReadUInt32(),z=reader.ReadUInt32(),w=reader.ReadUInt32()};
                    var b=ReadVector(reader);var o=ReadVector(reader);var p=ReadVector(reader);var n=ReadVector(reader);
                    for(int i=0;i<4;++i)reader.ReadInt32(); // Source fixture options.
                    var gloss=ReadVector(reader);int bypass=reader.ReadInt32();for(int i=0;i<3;++i)reader.ReadInt32();
                    var expected=new[]{ReadVector(reader),ReadVector(reader)};
                    // Repeat every fixture across all view/scope combinations.
                    // Inactive slots deliberately contain no lights or ambient.
                    for(int view=0;view<2;++view)for(int scope=0;scope<4;++scope) {
                        Array.Clear(words,0,words.Length);Array.Copy(raw,0,words,(view*4+scope)*39,39);lights.SetData(words);
                        shader.SetInt("_IdasView",view);shader.SetInt("_LightScope",scope);
                        shader.SetVector("_FixtureBase",b);shader.SetVector("_FixtureOffset",o);
                        shader.SetVector("_FixturePosition",p);shader.SetVector("_FixtureNormal",n);
                        shader.SetFloat("glossCoefficient",gloss.x);shader.SetInt("_FixtureBypass",bypass);
                        shader.Dispatch(kernel,1,1,1);output.GetData(actual);
                        for(int color=0;color<2;++color)for(int channel=0;channel<4;++channel) {
                            float e=expected[color][channel],a=actual[color][channel],diff=Math.Abs(e-a);
                            report.maximumError=Math.Max(report.maximumError,diff);++report.comparisons;
                            if(float.IsNaN(a)||float.IsInfinity(a)||diff>report.tolerance) {
                                ++report.failures;if(report.failures<1000)mismatch.WriteLine(FormattableString.Invariant($"{test},{view},{scope},{color},{channel},{e:R},{a:R},{diff:R}"));
                            }
                        }
                        ++report.cases;
                    }
                }
                if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new Exception("Trailing fixture bytes");
            }
            report.passed=report.failures==0;
            if(!report.passed)throw new Exception("Unity lighting differs from primary reference; see mismatches.csv");
        } catch(Exception e) { report.error=e.ToString();throw; }
        finally { File.WriteAllText(Path.Combine(folder,"summary.json"),JsonUtility.ToJson(report,true)); }
        Debug.Log($"PASS Unity lighting parity: {report.cases} cases, {report.comparisons} comparisons, max error {report.maximumError:R}");
    }
}

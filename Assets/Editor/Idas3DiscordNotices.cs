using System.IO;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;

// Keep third-party notices in every distributed player, including incremental builds.
public sealed class Idas3DiscordNotices : IPostprocessBuildWithReport
{
    public int callbackOrder => 100;
    public void OnPostprocessBuild(BuildReport report)
    {
        string folder=Path.Combine(Path.GetDirectoryName(report.summary.outputPath),Path.GetFileNameWithoutExtension(report.summary.outputPath)+"_Data","Plugins");
        Directory.CreateDirectory(folder);
        File.WriteAllText(Path.Combine(folder,"Discord-LICENSES.txt"),File.ReadAllText("Assets/Plugins/Discord/UPSTREAM.txt")+"\n\n"+File.ReadAllText("Assets/Plugins/Discord/DiscordRPC-LICENSE.txt")+"\n\n"+File.ReadAllText("Assets/Plugins/Discord/Newtonsoft.Json-LICENSE.txt"));
    }
}

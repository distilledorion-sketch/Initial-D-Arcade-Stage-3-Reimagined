using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Initial D Arcade Stage 3 Reimagined")]
[assembly: System.Reflection.AssemblyProduct("Initial D Arcade Stage 3 Reimagined")]

internal static class PortableLauncher
{
    private static bool SameLauncher(string first, string second)
    {
        using (var hash = System.Security.Cryptography.SHA256.Create())
        using (var a = File.OpenRead(first))
        using (var b = File.OpenRead(second))
            return a.Length == b.Length && Convert.ToBase64String(hash.ComputeHash(a)) == Convert.ToBase64String(hash.ComputeHash(b));
    }

    [STAThread]
    private static int Main(string[] args)
    {
        string root = AppDomain.CurrentDomain.BaseDirectory;
        try
        {
            string json = new JavaScriptSerializer().Serialize(args);
            string forwarded = Environment.GetEnvironmentVariable("IDAS3_RENAMED_LAUNCH_ARGS");
            Environment.SetEnvironmentVariable("IDAS3_RENAMED_LAUNCH_ARGS", null);
            string encoded = Convert.ToBase64String(Encoding.UTF8.GetBytes(json));
            string legacy = Path.Combine(root, "InitialDUnity.exe");
            string self = System.Reflection.Assembly.GetExecutingAssembly().Location;
            if (String.Equals(self, legacy, StringComparison.OrdinalIgnoreCase)) {
                if (forwarded != null && args.Length == 0) encoded = forwarded;
            } else if (File.Exists(legacy) && !SameLauncher(self, legacy)) {
                var migrate = new ProcessStartInfo(legacy) { WorkingDirectory = root, UseShellExecute = false };
                migrate.EnvironmentVariables["IDAS3_RENAMED_LAUNCH_ARGS"] = encoded;
                using (Process.Start(migrate)) { }
                return 0;
            }
            string script = Path.Combine(root, "InitialDUnity_Data", "GameSetup", "Play.ps1");
            var start = new ProcessStartInfo {
                FileName = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe"),
                Arguments = "-NoProfile -STA -ExecutionPolicy Bypass -File \"" + script + "\"",
                WorkingDirectory = root,
                UseShellExecute = false,
                CreateNoWindow = File.Exists(Path.Combine(root, "InitialDUnity_Data", "StreamingAssets", "IDAS3", "disc-receipt.json"))
            };
            start.EnvironmentVariables["IDAS3_LAUNCHER_PID"] = Process.GetCurrentProcess().Id.ToString();
            start.EnvironmentVariables["IDAS3_DISC_ARGS"] = encoded;
            using (var process = Process.Start(start))
            {
                process.WaitForExit();
                if (process.ExitCode != 0 && Array.IndexOf(args, "-batchmode") < 0 && Array.IndexOf(args, "-idas3-test-silent") < 0) {
                    string log = Path.Combine(root, "UserData", "Logs", "Launcher-error.txt");
                    string details = File.Exists(log) ? File.ReadAllText(log) : "See UserData\\Logs or run Play Reimagined.cmd for details.";
                    MessageBox.Show(details, "Initial D Arcade Stage 3 Reimagined", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return process.ExitCode;
            }
        }
        catch (Exception error)
        {
            MessageBox.Show(error.Message, "Initial D Arcade Stage 3 Reimagined", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}

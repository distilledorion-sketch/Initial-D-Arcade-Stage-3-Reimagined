using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Windows.Forms;

internal static class StandaloneInstall
{
    static string ChooseFolder()
    {
        using (var dialog = new FolderBrowserDialog()) {
            dialog.Description = "Choose your existing Initial D game folder or an empty folder for a new installation.";
            return dialog.ShowDialog() == DialogResult.OK ? dialog.SelectedPath : null;
        }
    }

    static int Run(string file, string arguments, string directory)
    {
        using (var child = Process.Start(new ProcessStartInfo(file, arguments) {
            WorkingDirectory = directory, UseShellExecute = false, CreateNoWindow = true
        })) {
            child.WaitForExit();
            return child.ExitCode;
        }
    }

    static void InstallFresh(FileStream archive, string root, string session)
    {
        string staged = Path.Combine(session, "new-game");
        Directory.CreateDirectory(staged);
        using (var zip = new ZipArchive(archive, ZipArchiveMode.Read, true)) {
            foreach (var entry in zip.Entries) {
                string name = Idas3UpdateStaging.SafeName(entry.FullName);
                string dest = Idas3UpdateStaging.Inside(staged, name);
                Directory.CreateDirectory(Path.GetDirectoryName(dest));
                using (var input = entry.Open())
                using (var output = new FileStream(dest, FileMode.CreateNew, FileAccess.Write))
                    input.CopyTo(output);
            }
        }
        if (Directory.Exists(root)) Directory.Delete(root, false);
        Directory.Move(staged, root);
        Directory.Delete(session, false);
    }

    [STAThread]
    static int Main(string[] args)
    {
        try {
            string root = args.Length == 0 ? ChooseFolder() : args[0];
            if (String.IsNullOrWhiteSpace(root)) return 0;
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            if (root.Length < 4 || Path.GetDirectoryName(root) == null) throw new IOException("Choose a game folder below the drive root.");
            Idas3UpdateStaging.NoLinks(root);
            string source = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, PackageIdentity.Name);
            if (!File.Exists(source))
                throw new IOException("Place the matching full Windows game ZIP beside Install Initial D Arcade Stage 3 Reimagined.exe and try again.");
            bool existing = File.Exists(Path.Combine(root, "InitialDUnity.exe"));
            if (existing) {
                string info = Path.Combine(root, "InitialDUnity_Data", "app.info");
                var identity = File.Exists(info) ? File.ReadAllLines(info) : new string[0];
                if (identity.Length != 2 || identity[0] != "Chris" ||
                    (identity[1] != "Initial D Unity" && identity[1] != "Initial D Arcade Stage 3 Reimagined"))
                    throw new IOException("Choose the existing Initial D game folder.");
            } else if (Directory.Exists(root) && Directory.EnumerateFileSystemEntries(root).Any())
                throw new IOException("Choose your existing game folder or an empty folder.");
            string session = existing
                ? Path.Combine(root, @"UserData\Temp\InitialDUpdates", Guid.NewGuid().ToString("N"))
                : Path.Combine(Path.GetDirectoryName(root), ".initial-d-install-" + Guid.NewGuid().ToString("N"));
            Idas3UpdateStaging.NoLinks(session);
            Directory.CreateDirectory(session);
            Console.WriteLine("Installing into " + root);
            if (existing) {
                string archive = Path.Combine(session, "game.zip");
                File.Copy(source, archive);
                string plan = Idas3UpdateStaging.Prepare(root, session, archive, PackageIdentity.Sha, false,
                    "", PackageIdentity.Version, 0, 0, null);
                string helper = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "UpdateInstaller.exe");
                if (Idas3UpdateStaging.Hash(helper) != PackageIdentity.HelperSha) throw new IOException("Extract the complete setup ZIP.");
                if (Run(helper, "\"" + plan + "\" --standalone", session) != 0) {
                    string error = Path.Combine(session, "error.txt");
                    throw new IOException(File.Exists(error) ? File.ReadAllText(error) : "Close the game and retry setup.");
                }
            } else {
                using (var input = File.Open(source, FileMode.Open, FileAccess.Read, FileShare.Read)) {
                    using (var hash = System.Security.Cryptography.SHA256.Create()) {
                        if (BitConverter.ToString(hash.ComputeHash(input)).Replace("-", "").ToLowerInvariant() != PackageIdentity.Sha)
                            throw new IOException("The Windows archive failed verification. Download the complete setup ZIP again.");
                    }
                    input.Position = 0;
                    InstallFresh(input, root, session);
                }
            }
            Console.WriteLine("Preparing the disc assets and portable player data...");
            string script = Path.Combine(root, @"InitialDUnity_Data\GameSetup\Play.ps1");
            string shell = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
            if (Run(shell, "-NoProfile -STA -ExecutionPolicy Bypass -File \"" + script + "\" -InstallOnly", root) != 0) {
                string log = Path.Combine(root, @"UserData\Logs\Launcher-error.txt");
                throw new IOException(File.Exists(log) ? File.ReadAllText(log) : "Run setup again to resume.");
            }
            Console.WriteLine("Installed. Open Initial D Arcade Stage 3 Reimagined.exe to play.");
            if (args.Length == 0) MessageBox.Show("Installation complete. Open Initial D Arcade Stage 3 Reimagined.exe to play.", "Initial D");
            return 0;
        } catch (Exception e) {
            Console.Error.WriteLine(e.Message);
            if (args.Length == 0) MessageBox.Show(e.Message, "Initial D setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}

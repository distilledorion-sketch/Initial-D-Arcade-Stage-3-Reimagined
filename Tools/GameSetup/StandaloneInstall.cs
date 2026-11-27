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
        string logFolder = AppDomain.CurrentDomain.BaseDirectory;
        string stage = "Choosing the installation folder";
        try {
            string root = args.Length == 0 ? ChooseFolder() : args[0];
            if (String.IsNullOrWhiteSpace(root)) return 0;
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            if (root.Length < 4 || Path.GetDirectoryName(root) == null) throw new IOException("Choose a game folder below the drive root.");
            Idas3UpdateStaging.NoLinks(root);
            stage = "Checking the Windows game package";
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
                logFolder = Path.Combine(root, @"UserData\Logs");
            } else if (Directory.Exists(root) && Directory.EnumerateFileSystemEntries(root).Any())
                throw new IOException("Choose your existing game folder or an empty folder.");
            stage = existing ? "Updating the existing installation" : "Unpacking the Windows game package";
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
                int updateCode = Run(helper, "\"" + plan + "\" --standalone", session);
                if (updateCode != 0) {
                    string error = Path.Combine(session, "error.txt");
                    throw new IOException("UpdateInstaller.exe exited with code " + updateCode + ". " +
                        (File.Exists(error) ? File.ReadAllText(error) : "Close the game and retry setup."));
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
            logFolder = Path.Combine(root, @"UserData\Logs");
            stage = "Preparing disc assets and player data";
            Console.WriteLine("Preparing the disc assets and portable player data...");
            string script = Path.Combine(root, @"InitialDUnity_Data\GameSetup\Play.ps1");
            string shell = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
            string notice = Path.Combine(logFolder, "Launcher-error.txt");
            DateTime previousNotice = File.GetLastWriteTimeUtc(notice);
            int setupCode = Run(shell, "-NoProfile -STA -ExecutionPolicy Bypass -File \"" + script + "\" -InstallOnly", root);
            if (setupCode != 0) {
                if (File.Exists(notice) && File.GetLastWriteTimeUtc(notice) != previousNotice)
                    return ShowFailure(File.ReadAllText(notice), args.Length == 0);
                throw new IOException("Play.ps1 exited with code " + setupCode + ".");
            }
            Console.WriteLine("Installed. Open Initial D Arcade Stage 3 Reimagined.exe to play.");
            if (args.Length == 0) MessageBox.Show("Installation complete. Open Initial D Arcade Stage 3 Reimagined.exe to play.", "Initial D");
            return 0;
        } catch (Exception e) {
            string message = e.Message;
            try {
                Idas3UpdateStaging.NoLinks(logFolder);
                Directory.CreateDirectory(logFolder);
                string report = Path.Combine(logFolder, "Setup-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff") +
                    "-" + Process.GetCurrentProcess().Id + ".log");
                File.WriteAllText(report, DateTime.UtcNow.ToString("o") + Environment.NewLine + stage +
                    Environment.NewLine + e.ToString());
                message += Environment.NewLine + report;
            } catch (Exception logError) {
                message += Environment.NewLine + "Could not save the setup log: " + logError.Message;
            }
            return ShowFailure(message, args.Length == 0);
        }
    }

    static int ShowFailure(string message, bool showDialog)
    {
        Console.Error.WriteLine(message);
        if (showDialog) MessageBox.Show(message, "Initial D setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
        return 1;
    }
}

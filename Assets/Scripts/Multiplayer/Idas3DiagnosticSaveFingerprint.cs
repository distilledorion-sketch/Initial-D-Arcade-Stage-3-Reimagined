using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace Idas3.Multiplayer
{
    // Diagnostic only. The platform's powered-on counter changes even when no
    // card/profile/stat is written. Validate its reserved root files separately;
    // every other file keeps the same byte-for-byte save protection.
    internal static class Idas3DiagnosticSaveFingerprint
    {
        internal static string Compute(string directory,bool includeResultReceipts,Action<bool,string> check)
        {
            var files=Directory.GetFiles(directory,"*",SearchOption.AllDirectories);
            Array.Sort(files,StringComparer.Ordinal);
            var text=new StringBuilder();
            using(var sha=SHA256.Create())foreach(string file in files){
                string relative=file.Substring(directory.Length+1).Replace('\\','/');
                if(relative=="platform-clock-v1.lock"){
                    // The native owner deliberately holds an exclusive handle.
                    check(new FileInfo(file).Length==0,"Platform clock lock unexpectedly contains data.");
                    continue;
                }
                if(relative=="platform-clock-v1.bin"){
                    check(new FileInfo(file).Length==16,"Platform clock snapshot has an invalid length.");continue;
                }
                if(relative=="platform-clock-v1.bin.new"){
                    ValidatePendingClock(file,check);continue;
                }
                if(!includeResultReceipts&&(relative=="online_result_v1.json"||relative=="online_result_v1.json.previous"))continue;
                if(!includeResultReceipts&&relative.EndsWith(".profile.lock",StringComparison.Ordinal)){
                    check(new FileInfo(file).Length==0,"Profile coordination lock unexpectedly contains progress data.");continue;
                }
                text.Append(relative).Append('=');
                using(var stream=File.OpenRead(file))text.Append(BitConverter.ToString(sha.ComputeHash(stream)).Replace("-",""));
                text.Append('\n');
            }
            return text.ToString();
        }

        private static void ValidatePendingClock(string file,Action<bool,string> check)
        {
            // This name may disappear between enumeration and open, or contain
            // a partially written record. Only the committed snapshot has a
            // stable checksum; the temporary write must stay bounded to16 bytes.
            // Query metadata only: even a delete-shared stream can interfere
            // with MoveFileEx publication on Windows. Native clock tests cover
            // header/integrity validation without disturbing the running owner.
            try{
                check(new FileInfo(file).Length<=16,"Pending platform clock snapshot exceeds its record size.");
            }catch(FileNotFoundException){ }
        }
    }
}

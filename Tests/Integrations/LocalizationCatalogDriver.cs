using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;

// Exercises built production translation/format methods with the source text
// catalog injected at its file boundary. No engine resource or font call runs.
internal static class LocalizationCatalogDriver
{
    static int checks;
    static void Check(bool value,string reason){if(!value)throw new Exception(reason);++checks;}
    static int Main(string[] args)
    {
        try{
            string managed=Path.GetFullPath(args[0]);
            AppDomain.CurrentDomain.AssemblyResolve+=(sender,eventArgs)=>{
                string path=Path.Combine(managed,new AssemblyName(eventArgs.Name).Name+".dll");
                return File.Exists(path)?Assembly.LoadFrom(path):null;
            };
            var game=Assembly.LoadFrom(Path.Combine(managed,"Assembly-CSharp.dll"));
            var localization=game.GetType("Idas3MenuLocalization",true);
            var catalog=new Dictionary<string,string[]>(StringComparer.OrdinalIgnoreCase);
            foreach(string line in File.ReadAllLines(args[1])){
                if(string.IsNullOrWhiteSpace(line)||line.StartsWith("#",StringComparison.Ordinal))continue;
                var fields=line.Split('\t');Check(fields.Length==3,"Translation row must have three columns");
                for(int i=0;i<3;++i)fields[i]=fields[i].Replace("\\n","\n");
                Check(!string.IsNullOrWhiteSpace(fields[0])&&!string.IsNullOrWhiteSpace(fields[1])&&!string.IsNullOrWhiteSpace(fields[2]),"Empty translation");
                var expected=Placeholders(fields[0]);
                Check(expected==Placeholders(fields[1])&&expected==Placeholders(fields[2]),"Placeholder mismatch: "+fields[0]);
                catalog.Add(fields[0],new[]{fields[1],fields[2]});
            }
            localization.GetField("catalog",BindingFlags.NonPublic|BindingFlags.Static).SetValue(null,catalog);
            var set=localization.GetMethod("SetLanguage");var translate=localization.GetMethod("T");var format=localization.GetMethod("Format");
            for(int language=0;language<3;++language){
                set.Invoke(null,new object[]{language});
                foreach(var row in catalog){
                    string expected=language==0?row.Key:row.Value[language-1];
                    Check((string)translate.Invoke(null,new object[]{row.Key})==expected,"Production translation differs for "+row.Key);
                }
                Check((string)translate.Invoke(null,new object[]{"Untranslated fixture text 1041"})=="Untranslated fixture text 1041","Unknown label changed");
                Check((string)translate.Invoke(null,new object[]{null})=="","Null label did not remain empty");
                string result=(string)format.Invoke(null,new object[]{"Connected to {0}.",new object[]{"READY"}});
                string expectedFormat=string.Format(language==0?"Connected to {0}.":catalog["Connected to {0}."][language-1],"READY");
                Check(result==expectedFormat&&result.Contains("READY"),"Opaque driver name was translated");
            }
            foreach(var label in new[]{"ONLINE","RETRY ONLINE","LAN DIRECT","QUICK MATCH","HOST PRIVATE BATTLE","Connecting to online services…","Finding another driver…","Online sign-in is reconnecting. Please retry shortly.","Room search failed: {0}","Find a driver with Quick Match, or create a room for friends.","PLAYER DETAILS","YOUR ONLINE PLAYER ID","CURRENT OPPONENT","RECENT OPPONENT","No online opponent yet.","Copy a Player ID when reporting a problem to the game moderators.","Player ID copied.","Not available"})
                Check(catalog.ContainsKey(label),"Missing current online label: "+label);
            var onlineLabels=new HashSet<string>(StringComparer.Ordinal);
            for(int file=2;file<args.Length;++file)
                foreach(Match match in Regex.Matches(File.ReadAllText(args[file]),"(?:Fail(?:Room)?\\(|(?:Status|deferredFailure|failure|Failure)\\s*=|return\\s+|throw new InvalidOperationException\\()\\s*\"((?:\\\\.|[^\"\\\\])*)\"")){
                    string label=Regex.Unescape(match.Groups[1].Value);
                    if(label.EndsWith(": ",StringComparison.Ordinal))label+="{0}";
                    else if(!label.EndsWith(".",StringComparison.Ordinal)&&!label.EndsWith("…",StringComparison.Ordinal))continue;
                    onlineLabels.Add(label);
                }
            foreach(string phase in new[]{"Finding another driver","Creating room","Joining room","Finding rooms","Connecting to driver","Checking driver access"}){
                onlineLabels.Add(phase+"…");onlineLabels.Add(phase+" timed out. Please retry.");
            }
            foreach(string label in new[]{"Online services are not initialized.","Online ready.","Online services are not connected.","No open rooms found. You can host one.","Found {0} open room(s).","Connected to {0}.","Room {0} — connecting to driver…","Room {0} — waiting for another driver."})onlineLabels.Add(label);
            foreach(string label in new[]{"Online play is restricted for this player. Contact the game administrator with your Player ID.","The other driver's online access is restricted.","Online access could not be verified. Please retry shortly.","Checking online access…","Verifying online access failed ({0})."})onlineLabels.Add(label);
            foreach(string label in onlineLabels)Check(catalog.ContainsKey(label),"Missing emitted online status: "+label);
            Console.WriteLine("PASS\t"+checks+" catalog/production localization checks");
            Console.WriteLine("ONLINE_STATUSES\t"+onlineLabels.Count);
            Console.WriteLine("ROWS\t"+catalog.Count);return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
    static string Placeholders(string value){
        var items=new List<string>();foreach(Match match in Regex.Matches(value,@"(?<!\{)\{(\d+)(?:[^{}]*)\}(?!\})"))items.Add(match.Groups[1].Value);
        items.Sort(StringComparer.Ordinal);return string.Join(",",items);
    }
}

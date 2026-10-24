using System;
using System.IO;
class LongPathCacheDriver {
    static int Main(string[] args) { try {
        if(args[0]=="safe") Idas3UpdateCache.MarkSafe(args[1]);
        else if(args[0]=="clean") {
            var result=Idas3UpdateCache.Clean(args[1],DateTime.UtcNow);
            Console.WriteLine(result.removed+" "+result.kept+" "+result.failed);
        } else if(args[0]=="lease") {
            using(Idas3UpdateCache.AcquireLease(args[1])) {
                Idas3UpdateCache.MarkSafe(args[1]);
                var result=Idas3UpdateCache.Clean(Path.GetDirectoryName(args[1]),DateTime.UtcNow);
                if(result.removed!=0||!Directory.Exists(Idas3UpdatePaths.FileSystemPath(args[1])))throw new Exception("Cleaner removed an active session");
            }
        } else throw new Exception("Unknown command");
        return 0;
    } catch(Exception error) {Console.Error.WriteLine(error);return 1;}}
}

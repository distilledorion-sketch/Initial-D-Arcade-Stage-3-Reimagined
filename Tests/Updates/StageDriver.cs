using System;
using System.IO;
using System.Web.Script.Serialization;
class StageDriver {
    static int Main(string[] a){try{
        var parser=new JavaScriptSerializer{MaxJsonLength=16*1024*1024};
        int done=-1,total=-1;
        string plan=Idas3UpdateStaging.Prepare(a[0],a[1],Path.Combine(a[1],"game.zip"),a[2],a[3]=="patch",a[4],a[5],int.Parse(a[6]),long.Parse(a[7]),json=>parser.Deserialize<Idas3UpdateStaging.Patch>(json),
            (current,count)=>{if(current!=done+1||count<=0||current>count||(total>=0&&count!=total))throw new Exception("Invalid staging progress.");done=current;total=count;});
        if(done!=total||total<=0)throw new Exception("Staging progress did not complete.");
        Console.WriteLine(plan);return 0;
    }catch(Idas3UpdateStaging.PatchRejectedException e){Console.Error.WriteLine(e.Message);return 2;}catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
}

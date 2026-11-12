using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

public sealed partial class Idas3CommunityTimes
{
    // One writer owns detached queue states. New finishes, upload replies and
    // sharing-off clears cannot race to replace the same pending.json file.
    sealed class PendingStore
    {
        readonly string directory;
        readonly CancellationToken stop;
        readonly int capacity;
        Pending current;
        long revision;
        Task<PendingCommit> tail;
        internal PendingStore(string directory,Pending initial,CancellationToken stop,int capacity){
            this.directory=directory;this.stop=stop;this.capacity=capacity;
            current=new Pending {runs=new List<Run>(initial.runs)};
        }
        Task<PendingCommit> Change(Func<Pending,Pending> change){
            var previous=tail;
            // Scheduling happens on the game thread; all file/state work below
            // runs in order on workers. No Unity objects are accessed there.
            return tail=Task.Run(async()=>{
                if(previous!=null)await previous.ConfigureAwait(false);
                try {
                    stop.ThrowIfCancellationRequested();
                    var next=change(current);bool changed=!ReferenceEquals(next,current);
                    if(changed){
                        WriteCache(directory,"pending.json",next,stop);
                        current=next;++revision;
                        try{CleanReplayFiles(directory);}catch(IOException){}catch(UnauthorizedAccessException){}
                    }
                    return new PendingCommit {state=current,revision=revision,changed=changed};
                }catch(Exception e){return new PendingCommit {state=current,revision=revision,error=e.GetType().Name};}
            });
        }
        internal Task<PendingCommit> Add(Run run)=>Change(before=>{
            var existing=before.runs.Find(r=>r.condition==run.condition&&r.weather==run.weather&&r.car==run.car);
            if(existing!=null&&existing.ticks6000<=run.ticks6000)return before;
            if(existing==null&&before.runs.Count>=capacity)throw new IOException("Upload queue is full.");
            var next=new Pending {runs=new List<Run>(before.runs)};
            if(existing!=null)next.runs.Remove(existing);next.runs.Add(run);return next;
        });
        internal Task<PendingCommit> Remove(string id)=>Change(before=>{
            if(!before.runs.Exists(r=>r.id==id))return before;
            var next=new Pending {runs=new List<Run>(before.runs)};next.runs.RemoveAll(r=>r.id==id);return next;
        });
        internal Task<PendingCommit> Clear()=>Change(before=>before.runs.Count==0?before:new Pending());
    }
    internal sealed class PendingCommit
    {
        internal Pending state;
        public long revision;
        public bool changed;
        public string error;
    }
    PendingStore pendingStore;
    long pendingRevision;
    bool stopping;
    Task<PendingCommit> finishWrite,clearWrite;
    string finishPayload;
    PendingStore QueueStore=>pendingStore??(pendingStore=new PendingStore(folder,pending,lifetime.Token,MaximumPendingRuns));
    internal Task<PendingCommit> QueueFinish(Run run){
        if(!Uploadable(run))throw new InvalidDataException("Invalid pending result.");
        // Freeze the submitted run before the worker sees it. Caller-owned
        // arrays or replay metadata must not change a pending write.
        var frozen=new Run {id=run.id,ruleset=run.ruleset,build=run.build,
            condition=run.condition,weather=run.weather,car=run.car,ticks6000=run.ticks6000,
            manual=run.manual,night=run.night,points=run.points,epoch=run.epoch,imported=run.imported,
            nameGlyphs=(int[])run.nameGlyphs.Clone(),splits=(int[])run.splits.Clone(),replayVersion=0,replayAvailable=false};
        return QueueStore.Add(frozen);
    }
    internal Task<PendingCommit> RemovePending(string id)=>QueueStore.Remove(id);
    internal Task<PendingCommit> ClearPending()=>QueueStore.Clear();
    internal bool PublishPending(Task<PendingCommit> work){
        if(stopping||!work.IsCompleted)return false;
        var commit=work.GetAwaiter().GetResult();
        // Coroutines can resume in a different order than writes completed.
        // An older acknowledgement must never restore an earlier queue view.
        if(commit.revision>=pendingRevision){pending=commit.state;pendingRevision=commit.revision;}
        return commit.error==null;
    }
    internal IEnumerator CommitPending(Task<PendingCommit> work,Action<bool> completed){
        while(!work.IsCompleted)yield return null;
        completed(PublishPending(work));
    }
}

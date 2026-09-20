using System;
using System.Collections.Generic;
using System.Globalization;

namespace Idas3.Multiplayer
{
    // Opt-in diagnostic at the real transport boundary. Each endpoint adds
    // half the requested RTT; only unreliable payloads are dropped/reordered.
    internal sealed class Idas3NetworkImpairment
    {
        sealed class Pending { public double Due; public long Order; public byte[] Data; public bool Reliable; }
        readonly List<Pending> queue=new List<Pending>();
        readonly Random random=new Random(31091);
        readonly Func<double> now;
        double reliableDue;
        long sequence;
        public readonly double RttMs,JitterMs,LossPercent;
        public long Dropped { get; private set; }
        public Idas3NetworkImpairment(Func<double> now)
        {
            this.now=now;RttMs=Argument("-idas3-net-rtt-ms",0,1000);
            JitterMs=Argument("-idas3-net-jitter-ms",0,200);LossPercent=Argument("-idas3-net-loss-pct",0,100);
        }
        static double Argument(string key,double fallback,double maximum)
        {
            var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,key);if(at<0)return fallback;
            if(at+1>=args.Length||!double.TryParse(args[at+1],NumberStyles.Float,CultureInfo.InvariantCulture,out double value)||double.IsNaN(value)||value<0||value>maximum)
                throw new ArgumentException("Invalid network diagnostic value for "+key);
            return value;
        }
        public void Send(IIdas3Transport transport,byte[] data,bool reliable)
        {
            if(RttMs==0&&JitterMs==0&&LossPercent==0){transport.Send(data,reliable);return;}
            if(!reliable&&random.NextDouble()*100<LossPercent){++Dropped;return;}
            if(queue.Count>=256)throw new InvalidOperationException("Diagnostic network queue overflow.");
            double due=now()+Math.Max(0,RttMs*.5+(random.NextDouble()*2-1)*JitterMs)/1000;
            if(reliable){due=Math.Max(due,reliableDue);reliableDue=due;}
            queue.Add(new Pending{Due=due,Order=++sequence,Data=data,Reliable=reliable});
        }
        public void Flush(IIdas3Transport transport)
        {
            queue.Sort((a,b)=>a.Due!=b.Due?a.Due.CompareTo(b.Due):a.Order.CompareTo(b.Order));
            while(queue.Count>0&&queue[0].Due<=now()){
                var item=queue[0];queue.RemoveAt(0);if(transport.Connected)transport.Send(item.Data,item.Reliable);
            }
        }
        public void Clear(){queue.Clear();reliableDue=0;}
    }
}

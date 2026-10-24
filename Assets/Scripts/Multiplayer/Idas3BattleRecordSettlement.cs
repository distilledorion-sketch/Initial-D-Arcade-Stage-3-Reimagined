using System;

namespace Idas3.Multiplayer
{
    // Owns one confirmed result from durable intent through both reward stores.
    // Native presentation succeeds once per live session; restart recovery runs
    // before the scene loads any profile into memory.
    internal sealed class Idas3BattleRecordSettlement
    {
        readonly int winner;
        readonly ulong hostTicks, guestTicks;
        readonly Idas3BattleRecord previous;
        readonly Idas3ResultJournal journal;
        readonly Idas3ResultJournal.Entry request;
        bool nativeApplied;
        double retryAt;
        public bool Complete { get; private set; }
        public Exception Failure { get; private set; }
        public Idas3BattleRecord Record { get; private set; }
        public int PointsEarned { get; private set; }
        public int TuningPointsEarned { get; private set; }
        public bool DurableIntent { get; private set; }
        public bool ChangesHistory => winner == 0 || winner == 1;

        internal Idas3BattleRecordSettlement(Idas3MultiplayerRecords records, int car,
            ulong raceId, int winner, bool won, Idas3BattleRecord opponent,
            Idas3BattleRecord previous, ulong hostTicks, ulong guestTicks)
        {
            if(records==null)throw new ArgumentNullException(nameof(records));
            this.winner = winner;this.previous = previous;
            this.hostTicks = hostTicks; this.guestTicks = guestTicks;
            journal = new Idas3ResultJournal(records);
            request = Idas3ResultJournal.Request(car,raceId,winner,won,opponent,previous,hostTicks,guestTicks);
        }

        public bool Matches(int result, ulong hostTime, ulong guestTime) =>
            winner == result && hostTicks == hostTime && guestTicks == guestTime;

        public bool TrySave(double now,bool force=false)
        {
            if (Complete) return true;
            if (!force && now < retryAt) return false;
            try
            {
                if (winner == -1)
                {
                    // Double timeout has neither tuning nor history to persist.
                    ApplyNative();Record=previous;
                }
                else {
                    var saved=journal.Settle(request,PrepareReward,_=>{DurableIntent=true;ApplyNative();});
                    Record=saved.record;PointsEarned=saved.battlePoints;
                }
                Complete = true; Failure = null;
                return true;
            }
            catch (Exception error)
            {
                Failure = error; retryAt = now + 1;
                return false;
            }
        }
        Idas3ResultJournal.Reward PrepareReward()
        {
            var before=new uint[307];var after=new uint[307];
            if(Idas3MultiplayerNative.Idas3MultiplayerPrepareResult(winner,out int selection,out uint points,before,after,307)!=1)
                throw new InvalidOperationException(Idas3Native.Error());
            return selection<0?new Idas3ResultJournal.Reward():new Idas3ResultJournal.Reward{
                selection=selection,points=points,before=before,after=after};
        }
        void ApplyNative()
        {
            if(nativeApplied)return;
            if(Idas3MultiplayerNative.Idas3MultiplayerSetResult(winner)!=1)
                throw new InvalidOperationException(Idas3Native.Error());
            nativeApplied=true;TuningPointsEarned=Idas3MultiplayerNative.Idas3MultiplayerPointsEarned();
        }
        internal static bool RecoverPending(string saveRoot)
        {
            var records=new Idas3MultiplayerRecords(saveRoot);
            return new Idas3ResultJournal(records).Recover(reward=>{
                if(reward.selection<0)return;
                var root=System.Text.Encoding.UTF8.GetBytes(records.SaveRoot+"\0");
                if(Idas3MultiplayerNative.Idas3MultiplayerRecoverReward(root,reward.selection,reward.points,reward.before,reward.after,307)!=1)
                    throw new InvalidOperationException(Idas3Native.Error());
            });
        }
    }
}

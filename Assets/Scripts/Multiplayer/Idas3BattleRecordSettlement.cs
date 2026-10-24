using System;

namespace Idas3.Multiplayer
{
    // Owns persistence for one already-confirmed result. Native tuning awards,
    // presentation and transport stay outside retries of the record file.
    internal sealed class Idas3BattleRecordSettlement
    {
        readonly Idas3MultiplayerRecords records;
        readonly int car, winner;
        readonly ulong raceId, hostTicks, guestTicks;
        readonly bool won;
        readonly Idas3BattleRecord opponent, previous;
        int? previousPoints;
        double retryAt;
        public bool Complete { get; private set; }
        public Exception Failure { get; private set; }
        public Idas3BattleRecord Record { get; private set; }
        public int PointsEarned { get; private set; }
        public bool ChangesHistory => winner == 0 || winner == 1;

        internal Idas3BattleRecordSettlement(Idas3MultiplayerRecords records, int car,
            ulong raceId, int winner, bool won, Idas3BattleRecord opponent,
            Idas3BattleRecord previous, ulong hostTicks, ulong guestTicks)
        {
            this.records = records ?? throw new ArgumentNullException(nameof(records));
            this.car = car; this.raceId = raceId; this.winner = winner; this.won = won;
            this.opponent = opponent; this.previous = previous;
            this.hostTicks = hostTicks; this.guestTicks = guestTicks;
        }

        public bool Matches(int result, ulong hostTime, ulong guestTime) =>
            winner == result && hostTicks == hostTime && guestTicks == guestTime;

        public bool TrySave(double now)
        {
            if (Complete) return true;
            if (now < retryAt) return false;
            try
            {
                if (ChangesHistory)
                {
                    if (!previousPoints.HasValue) previousPoints = records.ReadLevelPoints(car);
                    // Commit's durable receipt makes a repeat safe even when
                    // the write succeeded but a later read failed.
                    Record = records.Commit(car, raceId, won, opponent);
                    PointsEarned = records.ReadLevelPoints(car) - previousPoints.Value;
                }
                else Record = previous; // Draw/time-up never changes history.
                Complete = true; Failure = null;
                return true;
            }
            catch (Exception error)
            {
                Failure = error; retryAt = now + 1;
                return false;
            }
        }
    }
}

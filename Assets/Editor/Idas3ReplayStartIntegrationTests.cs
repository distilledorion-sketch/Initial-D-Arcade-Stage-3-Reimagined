using System;
using System.IO;
using UnityEngine;

// Explicit offline integration checks; neither test writes ordinary player saves.
public static class Idas3ReplayStartIntegrationTests
{
    public static void Run()
    {
        Idas3StartBindingChecks.Run();
        Idas3SteeringSourceChecks.Run();
        Idas3ReplayAnalysisTests.Run();
        Debug.Log("Replay/Start integration checks passed");
    }
    public static void Recorded()
    {
        string input=Environment.GetEnvironmentVariable("IDAS3_REPLAY_INTEGRATION_INPUT");
        string output=Environment.GetEnvironmentVariable("IDAS3_REPLAY_INTEGRATION_OUTPUT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output)||Directory.Exists(output))
            throw new InvalidOperationException("Native fixtures and a NEW private output directory required");
        Directory.CreateDirectory(output);
        var files=Directory.GetFiles(input,"*.idreplay");
        if(files.Length!=6&&files.Length!=12)throw new Exception("Missing both-slot native scenarios");
        foreach(var file in files){
            var replay=Idas3ReplayData.Load(file);
            if(!replay.Detailed||replay.Opponent==null||!replay.SteeringRecorded||!replay.RotationRecorded)
                throw new Exception("Confirmed revision4 replay lost analysis or its opponent: "+file);
            var analysis=new Idas3ReplayAnalysis(replay,replay.Opponent);
            if(!analysis.HasSpeedLoss||!analysis.HasSteeringSource||analysis.SpeedLoss.Length!=replay.Frames.Length)
                throw new Exception("Analysis did not retain native frame alignment: "+file);
            long count=0;foreach(var ticks in replay.Metadata.steeringTicks)count+=ticks;
            if(count!=replay.Frames.Length)throw new Exception("Recorded steering counts differ from confirmed poses");
            // Exercise the shipping library compression/package path, including
            // modern source smoke and backfire plus optional IDT1 analysis.
            var bytes=File.ReadAllBytes(file);int start=4+BitConverter.ToInt32(bytes,0);
            int telemetry=replay.Metadata.telemetryBytes,opponent=replay.Metadata.opponentBytes;
            int player=bytes.Length-start-opponent-telemetry;
            var saved=Idas3ReplayLibrary.Save(output,replay.Metadata,
                bytes[start..(start+player)],bytes[(start+player)..(start+player+opponent)],bytes[(start+player+opponent)..]);
            var restored=Idas3ReplayData.Load(saved);
            if(!restored.SteeringRecorded||restored.Frames.Length!=replay.Frames.Length||
                restored.SmokeFrames.Length!=replay.SmokeFrames.Length||restored.Frames[^1].backfire!=replay.Frames[^1].backfire)
                throw new Exception("Library round trip damaged effects or analysis");
            File.Copy(saved,Path.Combine(output,Path.GetFileName(file)),true);
        }
        Debug.Log("Recorded replay analysis/effects integration passed: "+files.Length+" scenarios");
    }
}

using System;
using System.Collections;
using UnityEngine;

namespace Idas3.Multiplayer {
    public sealed partial class Idas3MultiplayerSmoke {
        private bool ContactMotionCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-contact-motion-check")>=0;
        private bool contactHeadingSet;
        private float contactHeading;
        private Vector3 contactCenter;
        private bool ContactMotionKey(KeyCode key){
            var local=session.LocalSnapshot;var remote=session.RemoteSnapshot;
            if(!contactHeadingSet&&local.raceTicks>0){contactHeadingSet=true;contactHeading=local.yaw;contactCenter=(local.actor+remote.actor)*.5f;}
            ulong tick=local.raceTicks;
            // Normal bound pedals/steering only: merge onto the initial straight's
            // centre, brake the lead car and let the following car make contact.
            bool brake=role=="host"&&tick%480>=210&&tick%480<330;
            if(key==KeyCode.W)return !brake;
            if(key==KeyCode.S)return brake;
            if(!contactHeadingSet)return false;
            var right=new Vector3(Mathf.Cos(contactHeading),0,-Mathf.Sin(contactHeading));
            float lateral=Vector3.Dot(contactCenter-local.actor,right);
            float desired=contactHeading+Mathf.Clamp(lateral*.045f,-.13f,.13f);
            float error=Mathf.DeltaAngle(local.yaw*Mathf.Rad2Deg,desired*Mathf.Rad2Deg);
            // Positive bound steering decreases the rendered yaw.
            return error>1.2f?key==KeyCode.A:error< -1.2f&&key==KeyCode.D;
        }
        private IEnumerator RecordRenderedMotion(){
            var end=new WaitForEndOfFrame();
            while(!finished){yield return end;if(!finished&&session.IsRacing&&session.RaceReleased)RecordMotion();}
        }
    }
}

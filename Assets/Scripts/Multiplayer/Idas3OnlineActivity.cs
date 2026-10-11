using System;
using System.Globalization;

namespace Idas3.Multiplayer {
    public static class Idas3LobbyNames {
        public static string ForHost(string host){
            string name=string.IsNullOrWhiteSpace(host)?"Driver":host.Trim();
            var chars=name.ToCharArray();for(int i=0;i<chars.Length;++i)if(char.IsControl(chars[i])||chars[i]=='<'||chars[i]=='>')chars[i]=' ';
            return new string(chars,0,Math.Min(chars.Length,48)).Trim()+" Lobby";
        }
    }

    // A default snapshot means the service has no global population survey.
    // Room membership is not a count of all online players.
    public struct Idas3OnlineActivity {
        public int Online,Queuing,Racing;
        public bool Available,Limited;
        public double ObservedAt;
        internal static string StateFor(bool racing,string state,bool quick,bool inLobby,bool connected,bool disconnected)=>
            disconnected?"online":racing&&state!="Results"&&state!="Returning"?"racing":quick||inLobby&&!connected?"queuing":"online";
        public string Format(int count)=>Available?count.ToString(CultureInfo.InvariantCulture)+(Limited?"+":""):"—";
    }
}

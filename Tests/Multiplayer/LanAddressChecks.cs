using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using Idas3.Multiplayer;

// Exercise production adapter enumeration with isolated OS-boundary objects.
// No adapter, route, firewall or personal network settings are changed.
internal static class LanAddressChecks {
    sealed class Nic : NetworkInterface {
        readonly NetworkInterfaceType kind;readonly OperationalStatus status;readonly IPInterfaceProperties properties;
        internal bool Broken;
        internal Nic(NetworkInterfaceType type,bool up,string[] addresses,params string[] gateways){kind=type;status=up?OperationalStatus.Up:OperationalStatus.Down;properties=new Properties(addresses,gateways);}
        public override NetworkInterfaceType NetworkInterfaceType=>kind;
        public override OperationalStatus OperationalStatus=>status;
        public override IPInterfaceProperties GetIPProperties(){if(Broken)throw new NetworkInformationException();return properties;}
    }
    sealed class UnicastRecord : UnicastIPAddressInformation {
        readonly IPAddress address;
        internal UnicastRecord(string value){address=IPAddress.Parse(value);}
        public override IPAddress Address=>address;
        public override IPAddress IPv4Mask=>IPAddress.Parse("255.255.255.0");
        public override bool IsDnsEligible=>false;
        public override bool IsTransient=>false;
        public override long AddressPreferredLifetime=>long.MaxValue;
        public override long AddressValidLifetime=>long.MaxValue;
        public override long DhcpLeaseLifetime=>long.MaxValue;
        public override DuplicateAddressDetectionState DuplicateAddressDetectionState=>DuplicateAddressDetectionState.Preferred;
        public override PrefixOrigin PrefixOrigin=>PrefixOrigin.Manual;
        public override SuffixOrigin SuffixOrigin=>SuffixOrigin.Manual;
    }
    sealed class Gateway : GatewayIPAddressInformation {
        readonly IPAddress address;internal Gateway(string value){address=IPAddress.Parse(value);}
        public override IPAddress Address=>address;
    }
    sealed class Addresses : UnicastIPAddressInformationCollection {
        readonly List<UnicastIPAddressInformation> values;
        internal Addresses(string[] source){values=source.Select(s=>(UnicastIPAddressInformation)new UnicastRecord(s)).ToList();}
        public override int Count=>values.Count;
        public override UnicastIPAddressInformation this[int index]=>values[index];
        public override IEnumerator<UnicastIPAddressInformation> GetEnumerator()=>values.GetEnumerator();
    }
    sealed class Gateways : GatewayIPAddressInformationCollection {
        readonly List<GatewayIPAddressInformation> values;
        internal Gateways(string[] source){values=source.Select(s=>(GatewayIPAddressInformation)new Gateway(s)).ToList();}
        public override int Count=>values.Count;
        public override GatewayIPAddressInformation this[int index]=>values[index];
        public override IEnumerator<GatewayIPAddressInformation> GetEnumerator()=>values.GetEnumerator();
    }
    sealed class Properties : IPInterfaceProperties {
        readonly Addresses addresses;readonly Gateways gateways;
        internal Properties(string[] a,string[] g){addresses=new Addresses(a);gateways=new Gateways(g);}
        public override UnicastIPAddressInformationCollection UnicastAddresses=>addresses;
        public override GatewayIPAddressInformationCollection GatewayAddresses=>gateways;
        public override bool IsDnsEnabled=>false;
        public override string DnsSuffix=>"";
        public override bool IsDynamicDnsEnabled=>false;
        public override MulticastIPAddressInformationCollection MulticastAddresses=>throw new NotSupportedException();
        public override IPAddressInformationCollection AnycastAddresses=>throw new NotSupportedException();
        public override IPAddressCollection DnsAddresses=>throw new NotSupportedException();
        public override IPAddressCollection DhcpServerAddresses=>throw new NotSupportedException();
        public override IPAddressCollection WinsServersAddresses=>throw new NotSupportedException();
        public override IPv4InterfaceProperties GetIPv4Properties()=>throw new NotSupportedException();
        public override IPv6InterfaceProperties GetIPv6Properties()=>throw new NotSupportedException();
    }
    internal static void Run(){
        int failures=0;
        Action<string,string,NetworkInterface[]> check=(label,expected,adapters)=>{
            var actual=Idas3TcpTransport.LanAddress(adapters);
            if(actual==expected)Console.WriteLine("PASS\t"+label);
            else{++failures;Console.WriteLine("FAIL\t"+label+": expected "+(expected??"<none>")+", got "+(actual??"<none>"));}
        };
        var wifi=new Nic(NetworkInterfaceType.Wireless80211,true,new[]{"192.168.1.20"},"192.168.1.1");
        var offline=new Nic(NetworkInterfaceType.Ethernet,true,new[]{"192.168.50.2"});
        var tunnel=new Nic(NetworkInterfaceType.Tunnel,true,new[]{"10.8.0.2"},"10.8.0.1");
        check("Ordinary Wi-Fi retains its advertised address","192.168.1.20",new NetworkInterface[]{wifi});
        check("Gateway-free direct LAN advertises its real address","192.168.50.2",new NetworkInterface[]{offline});
        check("Connected Wi-Fi is preferred over a VPN tunnel listed first","192.168.1.20",new NetworkInterface[]{tunnel,wifi});
        check("Gateway-free Ethernet remains usable beside a VPN","192.168.50.2",new NetworkInterface[]{tunnel,offline});
        check("VPN-only private networks remain supported","10.8.0.2",new NetworkInterface[]{tunnel});
        check("No adapters never advertises localhost as a shared room",null,new NetworkInterface[0]);
        check("IPv6-only adapters do not produce a false IPv4 room",null,new NetworkInterface[]{new Nic(NetworkInterfaceType.Wireless80211,true,new[]{"fe80::1234"},"fe80::1")});
        check("Disconnected and loopback adapters are ignored",null,new NetworkInterface[]{new Nic(NetworkInterfaceType.Ethernet,false,new[]{"192.168.1.9"},"192.168.1.1"),new Nic(NetworkInterfaceType.Loopback,true,new[]{"127.0.0.1"})});
        check("One unreadable adapter does not hide working Wi-Fi","192.168.1.20",new NetworkInterface[]{new Nic(NetworkInterfaceType.Ethernet,true,new[]{"192.168.3.2"}){Broken=true},wifi});
        check("Unspecified multicast and loopback addresses are not advertised","192.168.1.20",new NetworkInterface[]{new Nic(NetworkInterfaceType.Ethernet,true,new[]{"0.0.0.0","127.0.0.1","224.0.0.1","255.255.255.255","192.168.1.20"},"192.168.1.1")});
        check("Link-local Ethernet supports a direct cable without a router","169.254.5.6",new NetworkInterface[]{new Nic(NetworkInterfaceType.Ethernet,true,new[]{"169.254.5.6"})});
        check("Routed Wi-Fi is preferred over an unrelated gateway-free Ethernet","192.168.1.20",new NetworkInterface[]{offline,wifi});
        if(failures!=0)throw new InvalidOperationException(failures+" LAN address checks failed");
    }
}

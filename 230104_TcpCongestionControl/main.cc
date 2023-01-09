#include <filesystem>
#include <iostream>
#include <map>
#include <string>

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Main");

//
// n0 --- n1 --- n2
//

class NetSim {
    std::string OUTPUT_DIR = "output/tcp-congestion-control/";
    double EPS = 1e-9;

    std::string m_tcpType;
    bool m_verbose;
    bool m_tracing;
    std::string m_prefix;

    NodeContainer m_nodes;
    NetDeviceContainer m_devices1;
    NetDeviceContainer m_devices2;
    Ipv4InterfaceContainer m_ifs1;
    Ipv4InterfaceContainer m_ifs2;
    double m_simStop;   // m_simStop:=(シミュレーション終了時間)．
    double m_ftpStart;  // m_ftpStart:=(FTP起動時間).
    double m_ftpStop;   // m_ftpStop:=(FTP停止時間).

    std::map<std::string, Ptr<OutputStreamWrapper> > m_streams;
    uint32_t m_cwnd;
    uint32_t m_ssth;

    void TraceMacTxRx(std::string context, Ptr<const Packet> packet);
    void TraceCwnd(std::string context, uint32_t oldValue, uint32_t newValue);
    void TraceSsth(std::string context, uint32_t oldValue, uint32_t newValue);
    void TraceCongState(std::string context, const TcpSocketState::TcpCongState_t oldValue, const TcpSocketState::TcpCongState_t newValue);
    void StartTracing(void);

    void CreateNodes(void);
    void ConfigureL2(void);
    void ConfigureL3(void);
    void ConfigureL7(void);

public:
    NetSim(int argc, char *argv[]);
    ~NetSim();

    void Run(void);
};

NetSim::NetSim(int argc, char *argv[]) {
    NS_LOG_FUNCTION(this << argc);

    m_tcpType = "TcpNewReno";
    m_prefix = m_tcpType;

    m_verbose = false;
    m_tracing = false;
    m_simStop = 100.0;
    m_ftpStart = 10.0;
    m_ftpStop = 90.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpType", "TCP Type to use", m_tcpType);
    cmd.AddValue("verbose", "Enable verbose logging if true", m_verbose);
    cmd.AddValue("tracing", "Enable tracing if true", m_tracing);
    cmd.AddValue("prefix", "prefix of output files", m_prefix);
    cmd.Parse(argc, argv);
}

NetSim::~NetSim() {
    NS_LOG_FUNCTION(this);
}

void NetSim::TraceMacTxRx(std::string context, Ptr<const Packet> packet) {
    NS_LOG_FUNCTION(this << context << packet);

    std::ostringstream oss;
    packet->Print(oss);

    NS_LOG_UNCOND("packet (uid: " << packet->GetUid() << ", size: " << packet->GetSize() << "bytes)");
    NS_LOG_UNCOND(oss.str());
    NS_LOG_UNCOND("");
}

void NetSim::TraceCwnd(std::string context, uint32_t oldValue, uint32_t newValue) {
    NS_LOG_FUNCTION(this << context << oldValue << newValue);

    int nodeId, sockId;
    sscanf(context.c_str(), "/NodeList/%d/$ns3::TcpL4Protocol/SocketList/%d/CongestionWindow", &nodeId, &sockId);

    if(m_streams.find(context) == m_streams.end()) {
        std::ostringstream oss;
        oss << OUTPUT_DIR << m_prefix << "_cwnd_" << nodeId << "_" << sockId << ".csv";
        AsciiTraceHelper ascii;
        m_streams[context] = ascii.CreateFileStream(oss.str());
        *m_streams[context]->GetStream() << "time,newValue" << std::endl;
        // *m_streams[context]->GetStream() << m_ftpStart << "," << oldValue << std::endl;
    }
    *m_streams[context]->GetStream() << Simulator::Now().GetSeconds() << "," << newValue << std::endl;
    m_cwnd = newValue;

    std::ostringstream oss2;
    oss2 << "/NodeList/" << nodeId << "/$ns3::TcpL4Protocol/SocketList/" << sockId << "/SlowStartThreshold";
    if(m_streams.find(oss2.str()) != m_streams.end()) {
        *m_streams[oss2.str()]->GetStream() << Simulator::Now().GetSeconds() << "," << m_ssth << std::endl;
    }
}

void NetSim::TraceSsth(std::string context, uint32_t oldValue, uint32_t newValue) {
    NS_LOG_FUNCTION(this << context << oldValue << newValue);

    int nodeId, sockId;
    sscanf(context.c_str(), "/NodeList/%d/$ns3::TcpL4Protocol/SocketList/%d/SlowStartThreshold", &nodeId, &sockId);

    if(m_streams.find(context) == m_streams.end()) {
        std::ostringstream oss;
        oss << OUTPUT_DIR << m_prefix << "_ssth_" << nodeId << "_" << sockId << ".csv";
        AsciiTraceHelper ascii;
        m_streams[context] = ascii.CreateFileStream(oss.str());
        *m_streams[context]->GetStream() << "time,newValue" << std::endl;
        // *m_streams[context]->GetStream() << m_ftpStart << "," << oldValue << std::endl;
    }
    *m_streams[context]->GetStream() << Simulator::Now().GetSeconds() << "," << newValue << std::endl;
    m_ssth = newValue;

    std::ostringstream oss2;
    oss2 << "/NodeList/" << nodeId << "/$ns3::TcpL4Protocol/SocketList/" << sockId << "/CongestionWindow";
    if(m_streams.find(oss2.str()) != m_streams.end()) {
        *m_streams[oss2.str()]->GetStream() << Simulator::Now().GetSeconds() << "," << m_cwnd << std::endl;
    }
}

void NetSim::TraceCongState(std::string context, const TcpSocketState::TcpCongState_t oldValue, const TcpSocketState::TcpCongState_t newValue) {
    NS_LOG_FUNCTION(this << context << oldValue << newValue);

    int nodeId, sockId;
    sscanf(context.c_str(), "/NodeList/%d/$ns3::TcpL4Protocol/SocketList/%d/CongState", &nodeId, &sockId);

    if(m_streams.find(context) == m_streams.end()) {
        std::ostringstream oss;
        oss << OUTPUT_DIR << m_prefix << "_cong-state_" << nodeId << "_" << sockId << ".csv";
        AsciiTraceHelper ascii;
        m_streams[context] = ascii.CreateFileStream(oss.str());
        *m_streams[context]->GetStream() << "time,newValue" << std::endl;
        *m_streams[context]->GetStream() << m_ftpStart << "," << oldValue << std::endl;
    }
    *m_streams[context]->GetStream() << Simulator::Now().GetSeconds() << "," << newValue << std::endl;
}

void NetSim::StartTracing(void) {
    NS_LOG_FUNCTION(this);

    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&NetSim::TraceCwnd, this));
    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/SlowStartThreshold", MakeCallback(&NetSim::TraceSsth, this));
    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongState", MakeCallback(&NetSim::TraceCongState, this));
}

void NetSim::CreateNodes(void) {
    NS_LOG_FUNCTION(this);

    m_nodes.Create(3);
}

void NetSim::ConfigureL2(void) {
    NS_LOG_FUNCTION(this);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue");
    m_devices1 = p2p.Install(m_nodes.Get(0), m_nodes.Get(1));

    p2p.SetDeviceAttribute("DataRate", StringValue("800Kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    p2p.SetQueue("ns3::DropTailQueue");
    m_devices2 = p2p.Install(m_nodes.Get(1), m_nodes.Get(2));
}

void NetSim::ConfigureL3(void) {
    NS_LOG_FUNCTION(this);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    m_ifs1 = address.Assign(m_devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    m_ifs2 = address.Assign(m_devices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void NetSim::ConfigureL7(void) {
    NS_LOG_FUNCTION(this);

    uint16_t port = 12345;
    unsigned dataSize = 5'000'000;  // [bytes]
    unsigned sendSize = 500;        // [bytes]

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    auto sinkApp = sink.Install(m_nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(m_simStop - 1.0));

    BulkSendHelper ftp("ns3::TcpSocketFactory", InetSocketAddress(m_ifs2.GetAddress(1), port));
    ftp.SetAttribute("MaxBytes", UintegerValue(dataSize));
    ftp.SetAttribute("SendSize", UintegerValue(sendSize));
    auto ftpApp = ftp.Install(m_nodes.Get(0));
    ftpApp.Start(Seconds(m_ftpStart));
    ftpApp.Stop(Seconds(m_ftpStop));
}

void NetSim::Run(void) {
    NS_LOG_FUNCTION(this);

    Time::SetResolution(Time::NS);
    Packet::EnablePrinting();
    Packet::EnableChecking();

    LogComponentEnable("TcpCongestionOps", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    if(m_verbose) {
        LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + m_tcpType));

    CreateNodes();
    ConfigureL2();
    ConfigureL3();
    ConfigureL7();

    Simulator::Stop(Seconds(100.0));

    if(m_verbose) {
        Config::Connect("/NodeList/0/DeviceList/*/$ns3::PointToPointNetDevice/MacTx", MakeCallback(&NetSim::TraceMacTxRx, this));
        Config::Connect("/NodeList/2/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&NetSim::TraceMacTxRx, this));
    }

    if(m_tracing) {
        std::filesystem::create_directories(OUTPUT_DIR.c_str());
        Simulator::Schedule(Seconds(m_ftpStart + EPS), &NetSim::StartTracing, this);
    }

    FlowMonitorHelper flowmonHelper;
    auto flowmon = flowmonHelper.InstallAll();

    Simulator::Run();

    flowmon->CheckForLostPackets();
    auto classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    const auto &stats = flowmon->GetFlowStats();
    NS_LOG_UNCOND("--------------------------------------------------");
    for(const auto &[id, stat] : stats) {
        const auto &ft = classifier->FindFlow(id);
        NS_LOG_UNCOND("Flow " << id << " (" << ft.sourceAddress << ":" << ft.sourcePort << " -> " << ft.destinationAddress << ":" << ft.destinationPort << ")");
        NS_LOG_UNCOND("  Protocol:            " << (int)ft.protocol);
        NS_LOG_UNCOND("  Tx Packets:          " << stat.txPackets);
        NS_LOG_UNCOND("  Tx Bytes:            " << stat.txBytes << " bytes");
        NS_LOG_UNCOND("  Rx Packets:          " << stat.rxPackets);
        NS_LOG_UNCOND("  Rx Bytes:            " << stat.rxBytes << " bytes");
        NS_LOG_UNCOND("  Lost Packets:        " << stat.lostPackets);
        NS_LOG_UNCOND("  Packets Loss Ration: " << (double)stat.lostPackets / stat.txPackets * 100 << " %");
        NS_LOG_UNCOND("  Forwarded Times:     " << stat.timesForwarded);
        if(stat.rxPackets > 0) {
            NS_LOG_UNCOND("  Mean Delay:          " << Time(stat.delaySum / stat.rxPackets).As(Time::MS));
            NS_LOG_UNCOND("  Mean Jitter:         " << Time(stat.jitterSum / stat.rxPackets).As(Time::MS));
        }
        NS_LOG_UNCOND("  Throughput:          " << stat.rxBytes * 8 / (stat.timeLastRxPacket - stat.timeFirstTxPacket).GetSeconds() << " bps");
    }
    NS_LOG_UNCOND("--------------------------------------------------");

    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    LogComponentEnable("Main", LOG_LEVEL_LOGIC);
    LogComponentEnableAll(LOG_PREFIX_ALL);

    NetSim netsim(argc, argv);
    netsim.Run();

    return 0;
}

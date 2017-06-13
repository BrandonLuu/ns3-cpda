/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This is an example script for AODV manet routing protocol. 
 *
 * Authors: Pavel Boyko <boyko@iitp.ru>
 */
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/v4ping-helper.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <cmath>


// Simulation Parameters
#define NODE_NUM 6 // number of sensor nodes in the network
#define NODE_STEP 50 // step distance between nodes
#define TOTAL_TIME 5 // total simulation time


using namespace ns3;

/**
 * \brief Test script.
 * 
 * This script creates 1-dimensional grid topology and then ping last node from the first one:
 * 
 * [10.0.0.1] <-- step --> [10.0.0.2] <-- step --> [10.0.0.3] <-- step --> [10.0.0.4]
 * 
 * ping 10.0.0.4
 */
//=============================================================================
// CPDA PROTOTYPE
//=============================================================================
class Cpda {
public:
	Cpda();
	/// Configure script parameters, \return true on successful configuration
	bool Configure(int argc, char **argv);
	/// Run simulation
	void Run();
	/// Report results
	void Report(std::ostream & os);

private:
	// parameters
	/// Number of nodes
	uint32_t size;
	/// Distance between nodes, meters
	double step;
	/// Simulation time, seconds
	double totalTime;
	/// Write per-device PCAP traces if true
	bool pcap;
	/// Print routes if true
	bool printRoutes;

	// network
	NodeContainer nodes;
	NodeContainer rootNode;
	NodeContainer senNodes;
	NetDeviceContainer devices;
	Ipv4InterfaceContainer interfaces;

private:
	void CreateNodes();
	void CreateDevices();
	void InstallInternetStack();
	void InstallApplications();
};

//=============================================================================
// CPDA CODE
//=============================================================================
Cpda::Cpda() :
		size(NODE_NUM), step(NODE_STEP), totalTime(TOTAL_TIME), pcap(false), printRoutes(false) {
}

bool Cpda::Configure(int argc, char **argv) {
	// Enable AODV logs by default. Comment this if too noisy
	// LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

	SeedManager::SetSeed(12345);
	CommandLine cmd;

	cmd.AddValue("pcap", "Write PCAP traces.", pcap);
	cmd.AddValue("printRoutes", "Print routing table dumps.", printRoutes);
	cmd.AddValue("size", "Number of nodes.", size);
	cmd.AddValue("time", "Simulation time, s.", totalTime);
	cmd.AddValue("step", "Grid step, m", step);

	cmd.Parse(argc, argv);
	return true;
}

void Cpda::Run() {
//  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
	CreateNodes();
	CreateDevices();
	InstallInternetStack();
	//InstallApplications();

	std::cout << "Starting simulation for " << totalTime << " s ...\n";

	// Configure NetAnim
	AnimationInterface anim("cpda-anim.xml");

	// Configure root node
	anim.UpdateNodeDescription(rootNode.Get(0), "ROOT"); // animation line
	anim.UpdateNodeColor(rootNode.Get(0), 0, 255, 0); // Optional
	anim.UpdateNodeSize(rootNode.Get(0)->GetId(), 10, 10);

	// Configure sensor nodes
	for (uint32_t i = 0; i < senNodes.GetN(); ++i) {
		anim.UpdateNodeDescription(senNodes.Get(i), "STA"); // animation line
		anim.UpdateNodeColor(senNodes.Get(i), 255, 0, 0); // Optional
		anim.UpdateNodeSize(senNodes.Get(i)->GetId(), 10, 10);
	}

	Simulator::Stop(Seconds(totalTime));
	Simulator::Run();
	Simulator::Destroy();
}

void Cpda::Report(std::ostream &) {
}

void Cpda::CreateNodes() {
	std::cout << "Creating " << (unsigned) size << " nodes " << step << " m apart.\n";

	// Create root and sensor
	rootNode.Create(1);
	senNodes.Create(size);
	// Add root and sensor to a single nodes container
	nodes.Add(senNodes);
	nodes.Add(rootNode);

	// Name nodes
	for (uint32_t i = 1; i < size; ++i) {
		std::ostringstream os;
		os << "node-" << i;
		Names::Add(os.str(), nodes.Get(i));
	}

	// Place root in top middle of grid 3x3 grid
	MobilityHelper mobility;
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

	// Place the root node at (100,0)
	mobility.SetPositionAllocator("ns3::GridPositionAllocator",
			"MinX", DoubleValue(50.0),
			"MinY", DoubleValue(0.0));
	mobility.Install(rootNode);

	// Make grid of sensor nodes starting at (0,100)
	mobility.SetPositionAllocator("ns3::GridPositionAllocator",
			"MinX", DoubleValue(0.0),
			"MinY", DoubleValue(100.0),
			"DeltaX", DoubleValue(50),
			"DeltaY", DoubleValue(100),
			"GridWidth", UintegerValue(3),
			"LayoutType", StringValue("RowFirst"));
	mobility.Install(senNodes);
}

void Cpda::CreateDevices() {
	WifiMacHelper wifiMac;
	wifiMac.SetType("ns3::AdhocWifiMac");
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

	// Set wifi propgation Disk model with 100m
	YansWifiChannelHelper wifiChannel;
	//YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(150));
	wifiPhy.SetChannel(wifiChannel.Create());

	WifiHelper wifi;
	wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
			StringValue("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue(0));
	devices = wifi.Install(wifiPhy, wifiMac, nodes);

	if (pcap) {
		wifiPhy.EnablePcapAll(std::string("aodv"));
	}
}

void Cpda::InstallInternetStack() {
	AodvHelper aodv;
	// you can configure AODV attributes here using aodv.Set(name, value)
	InternetStackHelper stack;
	stack.SetRoutingHelper(aodv); // has effect on the next Install ()
	stack.Install(senNodes);

	// Set root node attribute
	aodv.Set("EnableQueryNode", BooleanValue(true));
	stack.SetRoutingHelper(aodv); // has effect on the next Install ()
	stack.Install(rootNode);

	Ipv4AddressHelper address;
	address.SetBase("10.0.0.0", "255.0.0.0");
	interfaces = address.Assign(devices);

	if (printRoutes) {
		Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(
				"aodv.routes", std::ios::out);
		aodv.PrintRoutingTableAllAt(Seconds(8), routingStream);
	}
}

void Cpda::InstallApplications() {
	V4PingHelper ping(interfaces.GetAddress(size - 1));
	ping.SetAttribute("Verbose", BooleanValue(true));

	ApplicationContainer p = ping.Install(nodes.Get(0));
	p.Start(Seconds(2.0));
	p.Stop(Seconds(totalTime) - Seconds(0.001));
}

//=============================================================================
// CPDA TAG PROTOTYPE - Probably don't need
//=============================================================================
class CpdaTag: public Tag {
public:
	static TypeId GetTypeId(void);
	virtual TypeId GetInstanceTypeId(void) const;
	virtual uint32_t GetSerializedSize(void) const;
	virtual void Serialize(TagBuffer i) const;
	virtual void Deserialize(TagBuffer i);
	virtual void Print(std::ostream &os) const;

	// these are our accessors to our tag structure
	void SetSimpleValue(uint8_t value);
	uint8_t GetSimpleValue(void) const;

private:
	uint8_t m_simpleValue;
};
//=============================================================================
// CPDA TAG CODE
//=============================================================================
TypeId CpdaTag::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::KeyTag").SetParent<Tag>().AddConstructor<
			CpdaTag>().AddAttribute("SimpleValue", "A simple value",
			EmptyAttributeValue(),
			MakeUintegerAccessor(&CpdaTag::GetSimpleValue),
			MakeUintegerChecker<uint8_t>());
	return tid;
}

TypeId CpdaTag::GetInstanceTypeId(void) const {
	return GetTypeId();
}
uint32_t CpdaTag::GetSerializedSize(void) const {
	return 1;
}
void CpdaTag::Serialize(TagBuffer i) const {
	i.WriteU8(m_simpleValue);
}
void CpdaTag::Deserialize(TagBuffer i) {
	m_simpleValue = i.ReadU8();
}
void CpdaTag::Print(std::ostream &os) const {
	os << "v=" << (uint32_t) m_simpleValue;
}
void CpdaTag::SetSimpleValue(uint8_t value) {
	m_simpleValue = value;
}
uint8_t CpdaTag::GetSimpleValue(void) const {
	return m_simpleValue;
}

//=============================================================================
// MAIN CODE
//=============================================================================
int main(int argc, char **argv) {
	Cpda test;
	if (!test.Configure(argc, argv))
		NS_FATAL_ERROR("Configuration failed. Aborted.");

	test.Run();
	test.Report(std::cout);
	return 0;
}


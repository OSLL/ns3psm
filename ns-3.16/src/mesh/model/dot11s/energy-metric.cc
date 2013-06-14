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
 * Authors: Kirill Andreev <andreev@iitp.ru>
 */

#include "energy-metric.h"
#include "ns3/wifi-remote-station-manager.h"
#include "ns3/wifi-mode.h"
#include "ns3/li-ion-energy-source.h"
namespace ns3 {
namespace dot11s {
NS_OBJECT_ENSURE_REGISTERED (EnergyMetricCalculator);
TypeId
EnergyMetricCalculator::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::dot11s::EnergyMetricCalculator")
    .SetParent<Object> ()
    .AddConstructor<EnergyMetricCalculator> ()
    .AddAttribute ( "TestLength",
                    "Rate should be estimated using test length.",
                    UintegerValue (1024),
                    MakeUintegerAccessor (
                      &EnergyMetricCalculator::SetTestLength),
                    MakeUintegerChecker<uint16_t> (1)
                    )
    .AddAttribute ( "Dot11MetricTid",
                    "TID used to calculate metric (data rate)",
                    UintegerValue (0),
                    MakeUintegerAccessor (
                      &EnergyMetricCalculator::SetHeaderTid),
                    MakeUintegerChecker<uint8_t> (0)
                    )
    .AddAttribute ( "Dot11sMeshHeaderLength",
                    "Length of the mesh header",
                    UintegerValue (6),
                    MakeUintegerAccessor (
                      &EnergyMetricCalculator::m_meshHeaderLength),
                    MakeUintegerChecker<uint16_t> (0)
                    )
  ;
  return tid;
}
EnergyMetricCalculator::EnergyMetricCalculator () :
m_overheadNanosec (0)
{
}

EnergyMetricCalculator::EnergyMetricCalculator (Ptr<LiIonEnergySource> es) :
m_overheadNanosec (0)
{
	 m_energySource = es;
}
void
EnergyMetricCalculator::SetHeaderTid (uint8_t tid)
{
  m_testHeader.SetDsFrom ();
  m_testHeader.SetDsTo ();
  m_testHeader.SetTypeData ();
  m_testHeader.SetQosTid (tid);
}
void
EnergyMetricCalculator::SetTestLength (uint16_t testLength)
{
  m_testFrame = Create<Packet> (testLength + 6 /*Mesh header*/ + 36 /*802.11 header*/);
}

uint32_t
EnergyMetricCalculator::CalculateMetric(Mac48Address peerAddress, Ptr<MeshWifiInterfaceMac> mac, uint32_t hopcount)
{
	fl::Engine* engine = new fl::Engine;
		engine->setName("qtfuzzylite");

		fl::InputVariable* inputRemainEnegry = new fl::InputVariable;
		inputRemainEnegry->setName("remain_energy");
		inputRemainEnegry->setRange(0.000, 1.000);

		inputRemainEnegry->addTerm(new fl::Gaussian("nizky", 0.000,0.350));
		inputRemainEnegry->addTerm(new fl::Bell("sredny", 0.700,0.100,1.000));
		inputRemainEnegry->addTerm(new fl::Gaussian("visoky", 1.000,0.200));
		engine->addInputVariable(inputRemainEnegry);

		fl::InputVariable* inputEnergyCost = new fl::InputVariable;
		inputEnergyCost->setName("transport_energy");
		inputEnergyCost->setRange(0.000, 1.000);

		inputEnergyCost->addTerm(new fl::Gaussian("low", 0.000,0.200));
		inputEnergyCost->addTerm(new fl::Gaussian("medium", 0.500,0.200));
		inputEnergyCost->addTerm(new fl::Gaussian("high", 1.000,0.200));
		engine->addInputVariable(inputEnergyCost);

		fl::InputVariable* inputAccessible = new fl::InputVariable;
		inputAccessible->setName("accessible_through_others");
		inputAccessible->setRange(0.000, 1.000);

		inputAccessible->addTerm(new fl::Gaussian("no", 0.000,0.100));
		inputAccessible->addTerm(new fl::Gaussian("maybe", 0.700,0.300));
		inputAccessible->addTerm(new fl::Gaussian("yes", 1.000,0.100));
		engine->addInputVariable(inputAccessible);

		fl::OutputVariable* outputVariable1 = new fl::OutputVariable;
		outputVariable1->setName("accept");
		outputVariable1->setRange(0.000, 1.000);
		outputVariable1->setDefaultValue(std::numeric_limits<fl::scalar>::quiet_NaN());
		outputVariable1->setLockDefuzzifiedValue(true);
		outputVariable1->setDefuzzifier(new fl::Centroid(500));
		outputVariable1->output()->setAccumulation(new fl::Maximum);

		outputVariable1->addTerm(new fl::Gaussian("no", 0.000,0.300));
		outputVariable1->addTerm(new fl::Gaussian("yes", 1.000,0.300));
		engine->addOutputVariable(outputVariable1);

		fl::RuleBlock* ruleblock1 = new fl::RuleBlock;
		ruleblock1->setName("");
		ruleblock1->setTnorm(new fl::Minimum);
		ruleblock1->setSnorm(new fl::Maximum);
		ruleblock1->setActivation(new fl::Minimum);


		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is low and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is low and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is low and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is medium and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is medium and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is medium and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is high and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is high and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is nizky and transport_energy is high and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is low and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is low and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is low and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is medium and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is medium and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is medium and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is high and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is high and accessible_through_others is maybe then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is sredny and transport_energy is high and accessible_through_others is yes then accept is no", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is low and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is low and accessible_through_others is maybe then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is low and accessible_through_others is yes then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is medium and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is medium and accessible_through_others is maybe then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is medium and accessible_through_others is yes then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is high and accessible_through_others is no then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is high and accessible_through_others is maybe then accept is yes", engine));
		ruleblock1->addRule(fl::MamdaniRule::parse(
			"if remain_energy is visoky and transport_energy is high and accessible_through_others is yes then accept is yes", engine));
		engine->addRuleBlock(ruleblock1);

	    //Ищем energySource для пира
		//peerAddress -- узел через который отправляем?
		//mac -- destination?
		//или наоборот?
		//не могу понять, как взять объект по мак-адресу.
		//Ptr<EnergySource> energySource = (peerAddress->getMeshPointDevice)->GetNode()->GetObject<EnergySource>();

		inputRemainEnegry->setInput(m_energySource->GetEnergyFraction());
		inputEnergyCost->setInput(m_energySource->CalculateTotalCurrent());
		inputAccessible->setInput(hopcount);

		engine->process();

	    fl::scalar out = outputVariable1->defuzzify();
	    //return outputVariable1->getTerm(0)->membership(out); //значение для нет
	    return outputVariable1->getTerm(1)->membership(out) * 100; //значение для да
}
} // namespace dot11s
} // namespace ns3

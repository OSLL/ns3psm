/*
 * applications-serialization-test.cc
 *
 *  Created on: Aug 3, 2013
 *      Author: lom
 */


#include "ns3/test.h"

#include "ns3/onoff-application.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/archive/polymorphic_text_oarchive.hpp>
#include <boost/archive/polymorphic_binary_oarchive.hpp>

#include <boost/serialization/nvp.hpp>

#include "ns3/object-factory.h"

using namespace ns3;

/**
 * Test for applications serilization for dynamic load balancing
 */

class ApplicationsSerializationTestCase : public TestCase
{
public:
  ApplicationsSerializationTestCase ();
  virtual ~ApplicationsSerializationTestCase ();

private:
  virtual void DoRun (void);

};

ApplicationsSerializationTestCase::ApplicationsSerializationTestCase ()
  : TestCase ("Test applications serialization")
{
}

ApplicationsSerializationTestCase::~ApplicationsSerializationTestCase ()
{
}

void ApplicationsSerializationTestCase::DoRun (void)
{
    std::ofstream ofs("test.txt");
    boost::archive::text_oarchive oa(ofs);

	OnOffApplication app;

	ObjectFactory objectFactory;
	objectFactory.SetTypeId ("ns3::OnOffApplication");

	Ptr<Application> app_p = objectFactory.Create<Application> ();

	std::cout << app.GetTypeId().GetName() << std::endl;
	oa << app;

	std::cout << app_p->GetInstanceTypeId().GetName() << std::endl;

    Application* app_pp = GetPointer(app_p);
    oa << app_pp;
}


class ApplicationsSerializationTestSuite : public TestSuite
{
public:
	ApplicationsSerializationTestSuite ();
};

ApplicationsSerializationTestSuite::ApplicationsSerializationTestSuite ()
  : TestSuite ("applications-serialization", UNIT)
{
  AddTestCase (new ApplicationsSerializationTestCase);
}

static ApplicationsSerializationTestSuite applicationsSerializationTestSuite;

BOOST_CLASS_EXPORT(ns3::OnOffApplication)



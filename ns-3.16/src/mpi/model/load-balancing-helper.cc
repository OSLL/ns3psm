/*
 * load-balancing-helper.cc
 *
 *  Created on: May 4, 2013
 *      Author: olya
 */

#include "load-balancing-helper.h"
#include "load-balancing-application.h"


namespace ns3 {

void
LoadBalancingHelper::Install ()
{
  m_application = new LoadBalancingApplication();
  m_application->SetStartTime (Seconds (0));
}

void
LoadBalancingHelper::Start ()
{
  m_application->Start ();
}

void
LoadBalancingHelper::IncNodeLoad (uint32_t context)
{
  m_application->IncNodeLoad (context);
}

} /* namespace ns3 */

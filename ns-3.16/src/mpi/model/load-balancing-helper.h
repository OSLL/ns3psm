/*
 * load-balancing-helper.h
 *
 *  Created on: May 4, 2013
 *      Author: olya
 */

#ifndef LOAD_BALANCING_HELPER_H_
#define LOAD_BALANCING_HELPER_H_

#include "load-balancing-application.h"
#include "ns3/application.h"
namespace ns3 {

class LoadBalancingHelper {

public:
  static void Install ();
  static void Start ();
  static void IncNodeLoad (uint32_t context);

private:
  static Ptr<LoadBalancingApplication> m_application;

};

} /* namespace ns3 */
#endif /* LOAD_BALANCING_HELPER_H_ */

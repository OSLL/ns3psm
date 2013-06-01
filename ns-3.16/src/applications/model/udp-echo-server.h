/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */

#ifndef UDP_ECHO_SERVER_H
#define UDP_ECHO_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/extended_type_info_typeid.hpp>

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup udpecho UdpEcho
 */

/**
 * \ingroup udpecho
 * \brief A Udp Echo server
 *
 * Every packet received is sent back.
 */
class UdpEchoServer : public Application 
{
	friend class boost::serialization::access;

public:
  static TypeId GetTypeId (void);
  UdpEchoServer ();
  virtual ~UdpEchoServer ();

  template<class Archiver>
  void serialize(Archiver& ar, const unsigned int) {
    std::cout << "serializ udpechoserver" << std::endl;
    ar & boost::serialization::base_object<Application>(*this);
    //ar & m_peer;
    //ar & m_connected;
    //ar & m_onTime;
    //ar & m_offTime;
    //ar & m_cbrRate;
    //ar & m_pktSize;
    //ar & m_residualBits;
    //ar & m_lastStartTime;
    //ar & m_maxBytes;
    //ar & m_totBytes;
    //ar & m_startStopEvent;
    //ar & m_sendEvent;
    //ar & m_sending;
  }

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  Ptr<Socket> m_socket6;
  Address m_local;
};

} // namespace ns3

BOOST_CLASS_EXPORT_KEY(ns3::UdpEchoServer);

BOOST_CLASS_TYPE_INFO(
    ns3::UdpEchoServer,
    boost::serialization::extended_type_info_typeid<ns3::UdpEchoServer>
);

#endif /* UDP_ECHO_SERVER_H */


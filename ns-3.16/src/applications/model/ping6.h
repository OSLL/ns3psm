/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007-2009 Strasbourg University
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
 * Author: Sebastien Vincent <vincent@clarinet.u-strasbg.fr>
 */

#ifndef PING6_H
#define PING6_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv6-address.h"

#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/extended_type_info_typeid.hpp>

namespace ns3
{

class Packet;
class Socket;

/**
 * \ingroup applications 
 * \defgroup ping6 Ping6
 */

/**
 * \ingroup ping6
 * \class Ping6
 * \brief A ping6 application.
 */
class Ping6 : public Application
{
	friend class boost::serialization::access;
public:
  /**
   * \brief Get the type ID.
   * \return type ID
   */
  static TypeId GetTypeId ();

  /**
   * \brief Constructor.
   */
  Ping6 ();

  /**
   * \brief Destructor.
   */
  virtual ~Ping6 ();

  /**
   * \brief Set the local address.
   * \param ipv6 the local IPv6 address
   */
  void SetLocal (Ipv6Address ipv6);

  /**
   * \brief Set the remote peer.
   * \param ipv6 IPv6 address of the peer
   */
  void SetRemote (Ipv6Address ipv6);

  /**
   * \brief Set the out interface index.
   * This is to send to link-local (unicast or multicast) address
   * when a node has multiple interfaces.
   * \param ifIndex interface index
   */
  void SetIfIndex (uint32_t ifIndex);

  /**
   * \brief Set routers for routing type 0 (loose routing).
   * \param routers routers addresses
   */
  void SetRouters (std::vector<Ipv6Address> routers);

  template<class Archiver>
  void serialize(Archiver& ar, const unsigned int) {
    std::cout << "serializ ping6 " << std::endl;
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
  /**
   * \brief Dispose this object;
   */
  virtual void DoDispose ();

private:
  /**
   * \brief Start the application.
   */
  virtual void StartApplication ();

  /**
   * \brief Stop the application.
   */
  virtual void StopApplication ();

  /**
   * \brief Schedule sending a packet.
   * \param dt interval between packet
   */
  void ScheduleTransmit (Time dt);

  /**
   * \brief Send a packet.
   */
  void Send ();

  /**
   * \brief Receive method.
   * \param socket socket that receive a packet
   */
  void HandleRead (Ptr<Socket> socket);

  /**
   * \brief Peer IPv6 address.
   */
  Ipv6Address m_address;

  /**
   * \brief Number of "Echo request" packets that will be sent.
   */
  uint32_t m_count;

  /**
   * \brief Number of packets sent.
   */
  uint32_t m_sent;

  /**
   * \brief Size of the packet.
   */
  uint32_t m_size;

  /**
   * \brief Intervall between packets sent.
   */
  Time m_interval;

  /**
   * \brief Local address.
   */
  Ipv6Address m_localAddress;

  /**
   * \brief Peer address.
   */
  Ipv6Address m_peerAddress;

  /**
   * \brief Local socket.
   */
  Ptr<Socket> m_socket;

  /**
   * \brief Sequence number.
   */
  uint16_t m_seq;

  /**
   * \brief Event ID.
   */
  EventId m_sendEvent;

  /**
   * \brief Out interface (i.e. for link-local communication).
   */
  uint32_t m_ifIndex;

  /**
   * \brief Routers addresses for routing type 0.
   */
  std::vector<Ipv6Address> m_routers;
};

} /* namespace ns3 */

BOOST_CLASS_EXPORT_KEY(ns3::Ping6);

BOOST_CLASS_TYPE_INFO(
    ns3::Ping6,
    boost::serialization::extended_type_info_typeid<ns3::Ping6>
);

#endif /* PING6_H */


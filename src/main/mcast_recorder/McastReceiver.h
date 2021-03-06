//**************************************************
//  Author:      Sunny Yan
//  Created On:  Apr 9, 2014
//**************************************************
#ifndef MCASTRECEIVER_H_
#define MCASTRECEIVER_H_

#include "SDateTime.h"
#include "BinaryTools.h"
#include <iostream>
#include <string>
#include <ctime>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/cstdint.hpp>
#include <boost/lexical_cast.hpp>
#include <locale>
#include <sys/time.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

using namespace std;
using namespace boost;

class McastReceiver
{
  public:
    static const unsigned int BUFFER_SIZE = 1500;
    McastReceiver(
      boost::asio::io_service&,
      const string &,
      const boost::asio::ip::address&,
      const short,
      boost::posix_time::ptime,
      const char *,
      const short);
    virtual ~McastReceiver();

    virtual void handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd) = 0;

  protected:
    boost::asio::ip::udp::socket    m_Socket;
    boost::asio::ip::udp::endpoint  m_SenderEndpoint;
    char                            m_Buffer[BUFFER_SIZE];
    string                          m_Name;
    boost::posix_time::ptime        m_ProgramStartTime;
    short                           m_Mode;
    BinaryRecorder                  m_BinaryRecorder;
};

class McastReceiverHKExSim : public McastReceiver {
  public:
    McastReceiverHKExSim(
      boost::asio::io_service&,
      const string &,
      const boost::asio::ip::address&,
      const short,
      boost::posix_time::ptime,
      const char *,
      const short);
    void handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd);
};

class McastReceiverRelTime : public McastReceiver {
  public:
    McastReceiverRelTime(
      boost::asio::io_service&,
      const string &,
      const boost::asio::ip::address&,
      const short,
      boost::posix_time::ptime,
      const char *,
      const short);
    void handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd);
};

class McastReceiverUnixTime : public McastReceiver {
  public:
    McastReceiverUnixTime(
      boost::asio::io_service&,
      const string &,
      const boost::asio::ip::address&,
      const short,
      boost::posix_time::ptime,
      const char *,
      const short);
    void handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd);
};

#endif /* MCASTRECEIVER_H_ */

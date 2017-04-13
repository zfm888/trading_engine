/*
 * RefreshProcessor.h
 *
 *  Created on: May 19, 2014
 *      Author: sunny
 */

#ifndef REFRESHPROCESSOR_H_
#define REFRESHPROCESSOR_H_

#include "../Logger/Logger.h"
#include "../SharedObjects.h"
#include "../SystemConfig.h"
#include "../OMD_Global.h"
#include "../OMD_Message_Headers.h"
#include "../Container/ExpandableCirBuffer4Msg.h"
#include "../Container/OrderBook.h"
#include "DataProcFunctions.h"
#include "../ThreadHealthMonitor.h"
#include <boost/shared_ptr.hpp>
#include <cstdio>
#include <cstring>

using namespace std;
using namespace boost;

class RefreshProcessor {
  public:
    RefreshProcessor(const unsigned short);
    void Run();
    virtual ~RefreshProcessor() {}
  private:
    shared_ptr<DataProcFunctions>      m_DataProcFunc;

    //Output related
    bool                               m_bOutputJson;
    bool                               m_bRunRefreshProcessor;
    bool                               m_PrintRefreshProcSeqNoAsInfo;
    bool                               m_PrintOrderBookAsInfo;

    //Shared objects
    ExpandableCirBuffer4Msg *          m_MsgCirBuf_RT;
    ExpandableCirBuffer4Msg *          m_MsgCirBuf_RF;
    shared_ptr<SharedObjects>          m_ShrObj;
    shared_ptr<SystemConfig>           m_SysCfg;
    shared_ptr<Logger>                 m_Logger;
    shared_ptr<ThreadHealthMonitor>    m_ThreadHealthMon;

    //Others
    unsigned short                     m_ChannelID;
    unsigned short                     m_RefreshProcSleepMillisec;
    char                               m_JsonBuffer[JSON_BUFFER_SIZE];
    uint32_t                           m_LastCheckedAdjSeqNo;
    bool                               m_MaintainRefreshActn;
    unsigned int                       m_RFMsgCirBufProtection;

    //--------------------------------------------------
    // Velocity of Refresh Complete coverage
    //--------------------------------------------------
    uint32_t                           m_RTSeqNo1;
    uint32_t                           m_RTSeqNo2;
};

#endif /* REFRESHPROCESSOR_H_ */


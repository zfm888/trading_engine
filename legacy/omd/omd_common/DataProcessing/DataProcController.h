/*
 * DataProcController.h
 *
 *  Created on: Apr 11, 2014
 *      Author: sunny
 */

#ifndef DATAPROCCONTROLLER_H_
#define DATAPROCCONTROLLER_H_

#include <vector>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>
#include "PreProcessor.h"
#include "RealTimeProcessor.h"
#include "RefreshProcessor.h"
#include "../Logger/Logger.h"

using namespace std;
using namespace boost;

class DataProcController {
  public:
    DataProcController();
    virtual ~DataProcController();
    void StartDataProcressing();

  private:
    boost::thread_group                m_BoostThreadGrp;

    //--------------------------------------------------
    // Various Processores
    //--------------------------------------------------
    ptr_vector<PreProcessor>           m_PreProcessors;
    ptr_vector<RealTimeProcessor>      m_RealTimeProcessors;
    ptr_vector<RefreshProcessor>       m_RefreshProcessors;

    //------------------------------
    // System objects
    //------------------------------
    shared_ptr<SystemConfig>  m_SysCfg;
    shared_ptr<Logger>        m_Logger;
};

#endif /* DATAPROCCONTROLLER_H_ */

#include "DataProcFunctions.h"

//--------------------------------------------------
// Factory method
//--------------------------------------------------
DataProcFunctions* DataProcFuncFactory::GetDataProcFunc(dma::Identity id)
{
  if      (id == dma::OMDC) return new DataProcFunctions_OMDC();
  else if (id == dma::OMDD) return new DataProcFunctions_OMDD();
  else if (id == dma::MDP ) return new DataProcFunctions_MDP ();
}

DataProcFunctions::DataProcFunctions()
{
  m_SysCfg    = SystemConfig::Instance();
  m_DataTrans = DataTransmission::Instance();
  m_Logger    = Logger::Instance();
}

//--------------------------------------------------
// omd_mdi
//--------------------------------------------------
void DataProcFunctions_OMDC::ProcessMessageForTransmission(const BYTE* pbMsg, unsigned short usMsgType)
{
  switch (usMsgType)
  {
    // case OMDC_SECURITY_DEFINITION:
    //   {
    //     OMDC_Security_Definition* osd = (OMDC_Security_Definition *) pbMsg;
    //     shrobj->AddSpreadTable(osd->SecurityCode, lexical_cast<unsigned short>(string(osd->SpreadTableCode)));
    //     break;
    //   }
    case OMDC_TRADE:
      {
        OMDC_Trade * ost = (OMDC_Trade *) pbMsg;
        if (m_DataTrans->CheckIfSubscribed(ost->SecurityCode))
        {
          m_DataTrans->NotifyTrade(ost->TradeID,ost->SecurityCode,(double)(ost->Price)/1000,(double)ost->Quantity);
        }
        break;
      }
      // case OMDC_TRADE_CANCEL:
    case OMDC_TRADE_TICKER:
      {
        OMDC_Trade_Ticker * ost = (OMDC_Trade_Ticker *) pbMsg;
        if (m_DataTrans->CheckIfSubscribed(ost->SecurityCode))
        {
          m_DataTrans->NotifyTrade(ost->TickerID,ost->SecurityCode,(double)(ost->Price)/1000,(double)ost->AggregateQuantity);
        }
        break;
      }
    default:  break;
  }
  return;
}
void DataProcFunctions_OMDD::ProcessMessageForTransmission(const BYTE* pbMsg, unsigned short usMsgType)
{
  switch (usMsgType)
  {
    case OMDD_TRADE:
      {
        OMDD_Trade * ost = (OMDD_Trade *) pbMsg;
        if (m_DataTrans->CheckIfSubscribed(ost->OrderbookID))
        {
          m_DataTrans->NotifyTrade(ost->TradeID,ost->OrderbookID,(double)(ost->Price)/1000,(double)ost->Quantity);
        }
        break;
      }
      // case OMDD_TRADE_AMENDMENT:
      // case OMDD_TRADE_STATISTICS:

    default:  break;
  }
  return;
}

//--------------------------------------------------
// OMD-C
//--------------------------------------------------
void DataProcFunctions_OMDC::OutputJsonToLog(const char * sCaller, const unsigned short usChnlID, boost::shared_ptr<Logger> logger, const BYTE *pbMsg, char caJsonBuffer[])
{
  unsigned short usMsgType = 0;
  usMsgType = READ_UINT16(pbMsg+2);
  OutputJsonToLog(sCaller, usChnlID, logger, pbMsg, usMsgType, caJsonBuffer, 0);
  return;
}

void DataProcFunctions_OMDC::ProcessOrderBookInstruction(const char *sCaller, boost::shared_ptr<Logger> logger, const BYTE* pbMsg, boost::shared_ptr<SharedObjects> shrobj, bool bPrintOrderBookAsInfo)
{
  // Special handling for Order Book
  OMDC_Aggregate_Order_Book_Update* aobu = (OMDC_Aggregate_Order_Book_Update*) pbMsg;
  OrderBook* ob = shrobj->GetOrderBookCache()->GetOrderBook(aobu->SecurityCode);

  for (unsigned int i = 0; i < aobu->NoEntries; ++i)
  {
    OMDC_Aggregate_Order_Book_Update_Details* aobud =
      (OMDC_Aggregate_Order_Book_Update_Details*)
      ((BYTE*)pbMsg+sizeof(OMDC_Aggregate_Order_Book_Update)+i*sizeof(OMDC_Aggregate_Order_Book_Update_Details));

    OrderBook::BASide side = (aobud->Side == 0 ? OrderBook::BID : OrderBook::ASK);

    switch (aobud->UpdateAction)
    {
      case 0: //New
        ob->Add(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 1: //Change
        ob->Change(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 2: //Delete
        ob->Delete(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 74: //Order book clear
        ob->Reset();
        break;
      default:
        break;
    }
  }

  //--------------------------------------------------
  // Output OrderBook to Log
  //--------------------------------------------------
  if (bPrintOrderBookAsInfo)
  {
    {
      stringstream ss;

      ss << "DataProcFunctions_OMDC: " << string(sCaller) << ", Security Code: " << aobu->SecurityCode << ", ";
      ss << ob->DumpJson(aobu->SecurityCode,OrderBook::BID) << endl;

      if (bPrintOrderBookAsInfo)
        logger->Write(Logger::INFO,ss.str().c_str());
      else
        logger->Write(Logger::DEBUG,ss.str().c_str());
    }

    {
      stringstream ss;

      ss << "DataProcFunctions_OMDC: " << string(sCaller) << ", Security Code: " << aobu->SecurityCode << ", ";
      ss << ob->DumpJson(aobu->SecurityCode,OrderBook::ASK) << endl;

      if (bPrintOrderBookAsInfo)
        logger->Write(Logger::INFO,ss.str().c_str());
      else
        logger->Write(Logger::DEBUG,ss.str().c_str());
    }
  }

  //--------------------------------------------------
  // omd_mdi Callback
  //--------------------------------------------------
  if (m_DataTrans->CheckIfSubscribed(aobu->SecurityCode))
  {
    ATU_MDI_marketfeed_struct mfs;
    ob->Dump(mfs);
    m_DataTrans->NotifyOrderBookUpdate(aobu->SecurityCode,mfs);
  }

  return;
}


void DataProcFunctions_OMDC::OutputJsonToLog(const char * sCaller, const unsigned short usChnlID, boost::shared_ptr<Logger> logger, const BYTE *pbMsg, unsigned short usMsgType, char caJsonBuffer[], unsigned long ulHKExSendTime)
{
  switch (usMsgType)
  {
    //--------------------------------------------------
    // OMD
    //--------------------------------------------------
    case OMD_LOGON                               :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Logon                              *) pbMsg), string("Logon"                              ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_LOGON_RESPONSE                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Logon_Response                     *) pbMsg), string("Logon_Response"                     ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_REFRESH_COMPLETE                    :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Refresh_Complete                   *) pbMsg), string("Refresh_Complete"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_RETRANSMISSION_REQUEST              :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Retransmission_Request             *) pbMsg), string("Retransmission_Request"             ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_RETRANSMISSION_RESPONSE             :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Retransmission_Response            *) pbMsg), string("Retransmission_Response"            ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_SEQUENCE_RESET                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Sequence_Reset                     *) pbMsg), string("Sequence_Reset"                     ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;

                                                    //--------------------------------------------------
                                                    // OMD-C
                                                    //--------------------------------------------------
    case OMDC_MARKET_DEFINITION                  :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Market_Definition                 *) pbMsg), string("Market_Definition"                  ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_SECURITY_DEFINITION                :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Security_Definition               *) pbMsg), string("Security_Definition"                ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_LIQUIDITY_PROVIDER                 :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Liquidity_Provider                *) pbMsg), string("Liquidity_Provider"                 ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_CURRENCY_RATE                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Currency_Rate                     *) pbMsg), string("Currency_Rate"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_TRADING_SESSION_STATUS             :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Trading_Session_Status            *) pbMsg), string("Trading_Session_Status"             ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_SECURITY_STATUS                    :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Security_Status                   *) pbMsg), string("Security_Status"                    ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_NEWS                               :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_News                              *) pbMsg), string("News"                               ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_ADD_ORDER                          :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Add_Order                         *) pbMsg), string("Add_Order"                          ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_MODIFY_ORDER                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Modify_Order                      *) pbMsg), string("Modify_Order"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_DELETE_ORDER                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Delete_Order                      *) pbMsg), string("Delete_Order"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_ADD_ODD_LOT_ORDER                  :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Add_Odd_Lot_Order                 *) pbMsg), string("Add_Odd_Lot_Order"                  ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_DELETE_ODD_LOT_ORDER               :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Delete_Odd_Lot_Order              *) pbMsg), string("Delete_Odd_Lot_Order"               ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_NOMINAL_PRICE                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Nominal_Price                     *) pbMsg), string("Nominal_Price"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_INDICATIVE_EQUILIBRIUM_PRICE       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Indicative_Equilibrium_Price      *) pbMsg), string("Indicative_Equilibrium_Price"       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_YIELD                              :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Yield                             *) pbMsg), string("Yield"                              ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_TRADE                              :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Trade                             *) pbMsg), string("Trade"                              ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_TRADE_CANCEL                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Trade_Cancel                      *) pbMsg), string("Trade_Cancel"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_TRADE_TICKER                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Trade_Ticker                      *) pbMsg), string("Trade_Ticker"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_AGGREGATE_ORDER_BOOK_UPDATE        :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Aggregate_Order_Book_Update       *) pbMsg), string("Aggregate_Order_Book_Update"        ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_BROKER_QUEUE                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Broker_Queue                      *) pbMsg), string("Broker_Queue"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_STATISTICS                         :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Statistics                        *) pbMsg), string("Statistics"                         ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_MARKET_TURNOVER                    :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Market_Turnover                   *) pbMsg), string("Market_Turnover"                    ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_CLOSING_PRICE                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Closing_Price                     *) pbMsg), string("Closing_Price"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_INDEX_DEFINITION                   :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Index_Definition                  *) pbMsg), string("Index_Definition"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDC_INDEX_DATA                         :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDC: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDC_Index_Data                        *) pbMsg), string("Index_Data"                         ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;

    default                                      :  break;
  }

  return;
}


//--------------------------------------------------
// OMD-D
//--------------------------------------------------
void DataProcFunctions_OMDD::OutputJsonToLog(const char * sCaller, const unsigned short usChnlID, boost::shared_ptr<Logger> logger, const BYTE *pbMsg, char caJsonBuffer[])
{
  unsigned short usMsgType = 0;
  usMsgType = READ_UINT16(pbMsg+2);
  OutputJsonToLog(sCaller, usChnlID, logger, pbMsg, usMsgType, caJsonBuffer, 0);
  return;
}


void DataProcFunctions_OMDD::ProcessOrderBookInstruction(const char *sCaller, boost::shared_ptr<Logger> logger, const BYTE* pbMsg, boost::shared_ptr<SharedObjects> shrobj, bool bPrintOrderBookAsInfo)
{
  // Special handling for Order Book
  OMDD_Aggregate_Order_Book_Update* aobu = (OMDD_Aggregate_Order_Book_Update*) pbMsg;
  OrderBook* ob = shrobj->GetOrderBookCache()->GetOrderBook(aobu->OrderbookID);

  for (unsigned int i = 0; i < aobu->NoEntries; ++i)
  {
    OMDD_Aggregate_Order_Book_Update_Details* aobud =
      (OMDD_Aggregate_Order_Book_Update_Details*)
      ((BYTE*)pbMsg+sizeof(OMDD_Aggregate_Order_Book_Update)+i*sizeof(OMDD_Aggregate_Order_Book_Update_Details));

    OrderBook::BASide side = (aobud->Side == 0 ? OrderBook::BID : OrderBook::ASK);

    switch (aobud->UpdateAction)
    {
      case 0: //New
        ob->Add(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 1: //Change
        ob->Change(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 2: //Delete
        ob->Delete(side,aobud->Price,aobud->PriceLevel,aobud->NumberOfOrders,aobud->AggregateQuantity);
        break;
      case 74: //Order book clear
        ob->Reset();
        break;
      default:
        break;
    }
  }

  //--------------------------------------------------
  // Output OrderBook to Log
  //--------------------------------------------------
  if (bPrintOrderBookAsInfo)
  {
    {
      stringstream ss;

      ss << "DataProcFunctions_OMDD: " << string(sCaller) << ", OrderBookID: " << aobu->OrderbookID<< ", ";
      ss << ob->DumpJson(aobu->OrderbookID,OrderBook::BID) << endl;

      if (bPrintOrderBookAsInfo)
        logger->Write(Logger::INFO,ss.str().c_str());
      else
        logger->Write(Logger::DEBUG,ss.str().c_str());
    }

    {
      stringstream ss;

      ss << "DataProcFunctions_OMDD: " << string(sCaller) << ", OrderbookID: " << aobu->OrderbookID<< ", ";
      ss << ob->DumpJson(aobu->OrderbookID,OrderBook::ASK) << endl;

      if (bPrintOrderBookAsInfo)
        logger->Write(Logger::INFO,ss.str().c_str());
      else
        logger->Write(Logger::DEBUG,ss.str().c_str());
    }
  }

  //--------------------------------------------------
  // omd_mdi Callback
  //--------------------------------------------------
  if (m_DataTrans->CheckIfSubscribed(aobu->OrderbookID))
  {
    ATU_MDI_marketfeed_struct mfs;
    ob->Dump(mfs);
    m_DataTrans->NotifyOrderBookUpdate(aobu->OrderbookID,mfs);
  }

  return;
}

void DataProcFunctions_OMDD::OutputJsonToLog(const char * sCaller, const unsigned short usChnlID, boost::shared_ptr<Logger> logger, const BYTE *pbMsg, unsigned short usMsgType, char caJsonBuffer[], unsigned long ulHKExSendTime)
{
  switch (usMsgType)
  {

    //--------------------------------------------------
    // OMD
    //--------------------------------------------------
    case OMD_LOGON                               :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Logon                              *) pbMsg), string("Logon"                              ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_LOGON_RESPONSE                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Logon_Response                     *) pbMsg), string("Logon_Response"                     ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_REFRESH_COMPLETE                    :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Refresh_Complete                   *) pbMsg), string("Refresh_Complete"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_RETRANSMISSION_REQUEST              :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Retransmission_Request             *) pbMsg), string("Retransmission_Request"             ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_RETRANSMISSION_RESPONSE             :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Retransmission_Response            *) pbMsg), string("Retransmission_Response"            ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMD_SEQUENCE_RESET                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMD_Sequence_Reset                     *) pbMsg), string("Sequence_Reset"                     ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;

                                                    //--------------------------------------------------
                                                    // OMD-D
                                                    //--------------------------------------------------
    case OMDD_COMMODITY_DEFINITION               :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Commodity_Definition              *) pbMsg), string("Commodity_Definition"               ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_CLASS_DEFINITION                   :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Class_Definition                  *) pbMsg), string("Class_Definition"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_SERIES_DEFINITION_BASE             :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Series_Definition_Base            *) pbMsg), string("Series_Definition_Base"             ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_SERIES_DEFINITION_EXTENDED         :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Series_Definition_Extended        *) pbMsg), string("Series_Definition_Extended"         ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_COMBINATION_DEFINITION             :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Combination_Definition            *) pbMsg), string("Combination_Definition"             ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_MARKET_STATUS                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Market_Status                     *) pbMsg), string("Market_Status"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_SERIES_STATUS                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Series_Status                     *) pbMsg), string("Series_Status"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_COMMODITY_STATUS                   :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Commodity_Status                  *) pbMsg), string("Commodity_Status"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_ADD_ORDER                          :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Add_Order                         *) pbMsg), string("Add_Order"                          ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_MODIFY_ORDER                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Modify_Order                      *) pbMsg), string("Modify_Order"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_DELETE_ORDER                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Delete_Order                      *) pbMsg), string("Delete_Order"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_AGGREGATE_ORDER_BOOK_UPDATE        :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Aggregate_Order_Book_Update       *) pbMsg), string("Aggregate_Order_Book_Update"        ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_ORDER_BOOK_CLEAR                   :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Order_Book_Clear                  *) pbMsg), string("Order_Book_Clear"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_QUOTE_REQUEST                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Quote_Request                     *) pbMsg), string("Quote_Request"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_TRADE                              :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Trade                             *) pbMsg), string("Trade"                              ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_TRADE_AMENDMENT                    :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Trade_Amendment                   *) pbMsg), string("Trade_Amendment"                    ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_TRADE_STATISTICS                   :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Trade_Statistics                  *) pbMsg), string("Trade_Statistics"                   ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_SERIES_STATISTICS                  :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Series_Statistics                 *) pbMsg), string("Series_Statistics"                  ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_CALCULATED_OPENING_PRICE           :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Calculated_Opening_Price          *) pbMsg), string("Calculated_Opening_Price"           ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_ESTIMATED_AVERAGE_SETTLEMENT_PRICE :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Estimated_Average_Settlement_Price*) pbMsg), string("Estimated_Average_Settlement_Price" ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_MARKET_ALERT                       :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Market_Alert                      *) pbMsg), string("Market_Alert"                       ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_OPEN_INTEREST                      :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Open_Interest                     *) pbMsg), string("Open_Interest"                      ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;
    case OMDD_IMPLIED_VOLATILITY                 :  logger->Write(Logger::INFO, "HKEx Send Time: %s, DataProcFunctions_OMDD: %s, ChannelID:%u. %s", (ulHKExSendTime == 0 ? "Nil" : SDateTime::fromUnixTimeToString(ulHKExSendTime, SDateTime::NANOSEC, SDateTime::GMT, SDateTime::HKT).c_str()), sCaller, usChnlID, printStruct( *((OMDD_Implied_Volatility                *) pbMsg), string("Implied_Volatility"                 ) , caJsonBuffer , JSON_BUFFER_SIZE  ).c_str() );   break;

    default                                      :  break;
  }

  return;
}


//--------------------------------------------------
// FIXME copied from Kenny
//--------------------------------------------------
void DataProcFunctions_MDP::HandleMDPRaw(const BYTE *pbPkt, const unsigned short channelID, const McastIdentifier::EMcastType mt, const uint16_t usMsgSize)
{
  VAL sMcastType = string((mt == McastIdentifier::REALTIME) ? "RT":"RF");

  mktdata::MessageHeader mdpMsgHdr;
  int iOffset = sizeof(MDP_Packet_Header);

  while (iOffset < usMsgSize)
  {
    int16_t iMsgSize = READ_INT16(&pbPkt[iOffset]);
    const size_t iWrap2 = iOffset + sizeof(int16_t);
    const size_t iWrap3 = iMsgSize - sizeof(int16_t);
    const size_t iWrap4 = iOffset + iMsgSize;

    //--------------------------------------------------
    m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Message Header: Msg size: %d", channelID, sMcastType.c_str(), iMsgSize);
    if (iMsgSize <= 0) break;

    //--------------------------------------------------
    // m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Message Header: iWrap2    %d", channelID, sMcastType.c_str(), iWrap2);
    // m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Message Header: iWrap3    %d", channelID, sMcastType.c_str(), iWrap3);
    // m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Message Header: iWrap4    %d", channelID, sMcastType.c_str(), iWrap4);

    mdpMsgHdr.wrap((char*)pbPkt, iWrap2, MDP_VERSION, iWrap4);

    //--------------------------------------------------
    m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Message Header: Msg type: %u", channelID, sMcastType.c_str(), mdpMsgHdr.templateId());

    switch (mdpMsgHdr.templateId())
    {
      case MDP_HEARTBEAT                          : OnHeartBeat                        (channelID, sMcastType);                                                break;
      case MDP_CHANNEL_RESET                      : OnChannelReset                     (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_SECURITY_DEFINITION_FUTURE : OnRefreshSecurityDefinitionFuture  (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_SECURITY_DEFINITION_SPREAD : OnRefreshSecurityDefinitionSpread  (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_SECURITY_STATUS                    : OnSecurityStatus                   (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_BOOK                       : OnRefreshBook                      (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_DAILY_STATISTICS           : OnRefreshDailyStatistics           (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_LIMITS_BANDING             : OnRefreshLimitsBanding             (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_SESSION_STATISTICS         : OnRefreshSessionStatistics         (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_TRADE                      : OnRefreshTrade                     (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_VOLUME                     : OnRefreshVolume                    (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_SNAPSHOT_FULL_REFRESH              : OnSnapshotFullRefresh              (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_QUOTE_REQUEST                      : OnQuoteRequest                     (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_INSTRUMENT_DEFINITION_OPTION       : OnInstrumentDefinitionOption       (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      case MDP_REFRESH_TRADE_SUMMARY              : OnRefreshTradeSummary              (channelID, mdpMsgHdr, (char*)pbPkt + iWrap2, iWrap3, sMcastType);      break;
      default                                     : m_Logger->Write(Logger::DEBUG,"Unprocessed message. %s templateId: %d", sMcastType.c_str(), mdpMsgHdr.templateId());  break;
    }

    //--------------------------------------------------
    // Process message for omd_mdi
    //--------------------------------------------------
    ProcessMessageForTransmission(pbPkt, mdpMsgHdr.templateId());
    //--------------------------------------------------

    iOffset += iMsgSize;
  }
  return;
}

vector<uint32_t> DataProcFunctions_MDP::Get_LastMsgSeqNumProcessed(const unsigned short channelID, const BYTE *pbPkt, const uint16_t usMsgSize)
{
  mktdata::MessageHeader mdpMsgHdr;
  int iOffset = sizeof(MDP_Packet_Header);

  vector<uint32_t> vRtn;
  while (iOffset < usMsgSize)
  {
    int16_t iMsgSize = READ_INT16(&pbPkt[iOffset]);
    const size_t iWrap2 = iOffset + sizeof(int16_t);
    const size_t iWrap3 = iMsgSize - sizeof(int16_t);
    const size_t iWrap4 = iOffset + iMsgSize;
    if (iMsgSize <= 0) break;
    mdpMsgHdr.wrap((char*)pbPkt, iWrap2, MDP_VERSION, iWrap4);
    if (mdpMsgHdr.templateId() == MDP_SNAPSHOT_FULL_REFRESH)
    {
      SnapshotFullRefresh38 msg;
      msg.wrapForDecode((char*)pbPkt+iWrap2, mdpMsgHdr.encodedLength(), mdpMsgHdr.blockLength(), MDP_VERSION, iWrap3);
      vRtn.push_back(msg.lastMsgSeqNumProcessed());
    }
    iOffset += iMsgSize;
  }
  return vRtn;
}

void DataProcFunctions_MDP::OnHeartBeat(const unsigned short channelID, const string & sMcastType)
{
  m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s Heartbeat", channelID, sMcastType.c_str());
  return;
}


void DataProcFunctions_MDP::OnRefreshVolume(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshVolume37 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    ostringstream oo;
    while (ent.hasNext())
    {
      ent.next();

      oo.clear();
      oo
        << "MDIncrementalRefreshVolume37: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " EntrySize: " << (int)(ent.mDEntrySize())
        << " UpdateAction: " << (int)(ent.mDUpdateAction())
        << " EntryType: " << string(ent.mDEntryType());

      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnRefreshBook(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshBook32 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  while (ent.hasNext())
  {
    ent.next();

    if (m_Logger->NeedToPrint(Logger::DEBUG))
    {
      ostringstream oo;
      oo
        << "MDIncrementalRefreshBook32: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " EntrySize: " << (int)(ent.mDEntrySize())
        << " UpdateAction: " << (int)(ent.mDUpdateAction())
        << " EntryType: " << (char)(ent.mDEntryType())
        << " transactTime: " << SDateTime::fromUnixTimeToString(msg.transactTime(), SDateTime::NANOSEC);

      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }

    if (ent.mDEntryType() == MDEntryTypeBook::Bid || ent.mDEntryType() == MDEntryTypeBook::Offer ||
        ent.mDEntryType() == MDEntryTypeBook::ImpliedBid || ent.mDEntryType() == MDEntryTypeBook::ImpliedOffer)
    {

      if (m_Logger->NeedToPrint(Logger::DEBUG))
      {
        ostringstream oo;
        oo
          << "MDIncrementalRefreshBook32: SecurityID: " << ent.securityID()
          << " mDEntryType: " << (char)(ent.mDEntryType())
          << " mDPriceLevel: " << (int)(ent.mDPriceLevel())
          << " mDUpdateAction: " << (int)(ent.mDUpdateAction())
          << " mDEntryPx.mantissa: " << (int64_t)(ent.mDEntryPx().mantissa())
          << " mDEntrySize: " << (int)(ent.mDEntrySize())
          << " numberOfOrders: " << (int)(ent.numberOfOrders())
          << " rptSeq: " << (int)(ent.rptSeq());
        m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
      }

      OrderBook* ob = SharedObjects::Instance()->GetOrderBookCache()->GetOrderBook(ent.securityID());
      OrderBook::BASide side = (((ent.mDEntryType() == MDEntryTypeBook::Bid) || (ent.mDEntryType() == MDEntryTypeBook::ImpliedBid)) ? OrderBook::BID : OrderBook::ASK);
      switch (ent.mDUpdateAction())
      {
        case MDUpdateAction::New:
          ob->Add(side,(int64_t)(ent.mDEntryPx().mantissa()),(int)(ent.mDPriceLevel()),(int)(ent.numberOfOrders()),(int)(ent.mDEntrySize()));
          break;
        case MDUpdateAction::Change:
          ob->Change(side,(int64_t)(ent.mDEntryPx().mantissa()),(int)(ent.mDPriceLevel()),(int)(ent.numberOfOrders()),(int)(ent.mDEntrySize()));
          break;
        case MDUpdateAction::Delete:
          ob->Delete(side,(int64_t)(ent.mDEntryPx().mantissa()),(int)(ent.mDPriceLevel()),(int)(ent.numberOfOrders()),(int)(ent.mDEntrySize()));
          break;
        case MDUpdateAction::DeleteThru:
          ob->Reset(side);
          break;
        case MDUpdateAction::DeleteFrom:
          ob->DeleteTopLevels(side,(int)(ent.mDPriceLevel()));
          break;
        default:
          break;
      }

    }
  }
}

void DataProcFunctions_MDP::OnInstrumentDefinitionOption(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDInstrumentDefinitionOption41 msg;
}


void DataProcFunctions_MDP::OnRefreshTrade(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshTrade36 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo
        << "MDIncrementalRefreshTrade36: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " Price: " << (int64_t)(ent.mDEntryPx().mantissa())
        << " EntrySize: " << (int)(ent.mDEntrySize())
        << " OrderNo: " << (int)(ent.numberOfOrders())
        << " AggressorSide: " << ent.aggressorSide()
        << " TradeID: " << ent.tradeID()
        << " UpdateAction: " << (int)(ent.mDUpdateAction())
        << " EntryType: " << string(ent.mDEntryType());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnRefreshTradeSummary(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshTradeSummary42 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);

  auto& order_id_ent = msg.noOrderIDEntries();
  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (order_id_ent.hasNext())
    {
      order_id_ent.next();

      ostringstream oo;
      oo << "MDIncrementalRefreshTradeSummary42: OrderID: " << order_id_ent.orderID()
        << " Qty: " << order_id_ent.lastQty();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }

  auto& ent = msg.noMDEntries();
  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDIncrementalRefreshTradeSummary42: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " Price: " << (int64_t)(ent.mDEntryPx().mantissa())
        << " EntrySize: " << (int)(ent.mDEntrySize())
        << " OrderNo: " << (int)(ent.numberOfOrders())
        << " AggressorSide: " << ent.aggressorSide()
        << " EntryType: " << string(ent.mDEntryType());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnRefreshDailyStatistics(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshDailyStatistics33 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDIncrementalRefreshDailyStatistics33: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " EntryPx: " << (int64_t)(ent.mDEntryPx().mantissa())
        << " EntryType: " << (char)(ent.mDEntryType())
        << " TradingDate: " << ent.tradingReferenceDate();

      switch(ent.mDEntryType())
      {
        case MDEntryTypeDailyStatistics::ClearedVolume:   { oo << "Cleared Volume: "; break;  }
        case MDEntryTypeDailyStatistics::SettlementPrice: {
                                                            oo << "Settlement Price: ";
                                                            if (ent.settlPriceType().finalfinal())   cout << "finalfinal ";
                                                            if (ent.settlPriceType().actual())       cout << "actual ";
                                                            if (ent.settlPriceType().rounded())      cout << "rounded ";
                                                            if (ent.settlPriceType().intraday())     cout << "intraday ";
                                                            if (ent.settlPriceType().reservedBits()) cout << "reservedBits ";
                                                            if (ent.settlPriceType().nullValue())    cout << "nullValue ";
                                                            break;
                                                          }
        case MDEntryTypeDailyStatistics::OpenInterest:    { oo << "Open Interest: "; break;   }
        case MDEntryTypeDailyStatistics::FixingPrice:     { oo << "Fixing Price: ";  break;   }
        case MDEntryTypeStatistics::OpenPrice:            { oo << "Open Price: ";    break;   }
        case MDEntryTypeStatistics::HighTrade:            { oo << "High Trade: ";    break;   }
        case MDEntryTypeStatistics::LowTrade:             { oo << "Low Trade: ";     break;   }
        case MDEntryTypeStatistics::HighestBid:           { oo << "Highest Bid: ";   break;   }
        case MDEntryTypeStatistics::LowestOffer:          { oo << "Lowest Offer: ";  break;   }
        default: break;
      }
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnRefreshSessionStatistics(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshSessionStatistics35 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDIncrementalRefreshSessionStatistics35: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " EntryPx: " << (int64_t)(ent.mDEntryPx().mantissa())
        << " EntryType: " << (char)(ent.mDEntryType())
        << " OpenCloseSettlFlag: " << (int)(ent.openCloseSettlFlag())
        << " UpdateAction: " << (int)(ent.mDUpdateAction());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }

  }
}

void DataProcFunctions_MDP::OnRefreshLimitsBanding(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDIncrementalRefreshLimitsBanding34 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDIncrementalRefreshLimitsBanding34: SecurityID: " << ent.securityID()
        << " RptSeq: " << (int)(ent.rptSeq())
        << " HighLimit: " << (int64_t)(ent.highLimitPrice().mantissa())
        << " LowLimit: " << (int64_t)(ent.lowLimitPrice().mantissa())
        << " MaxPriceVariation: " << (int64_t)(ent.maxPriceVariation().mantissa())
        << " UpdateAction: " << (int)(ent.mDUpdateAction())
        << " EntryType: " << string(ent.mDEntryType());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }

}

void DataProcFunctions_MDP::OnRefreshSecurityDefinitionFuture(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDInstrumentDefinitionFuture27 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noEvents();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDInstrumentDefinitionFuture27: "
        << " event time: " << SDateTime::fromUnixTimeToString(ent.eventTime(), SDateTime::NANOSEC)
        << " event type: " << ent.eventType()
        << " security ID: " << msg.securityID()
        << " symbol: " << msg.symbol()
        << " security group: " << msg.getSecurityGroupAsString()
        << " total number reports: " << msg.totNumReports()
        << " market segment ID: " << msg.marketSegmentID();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }

  auto& feed_types = msg.noMDFeedTypes();
  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (feed_types.hasNext())
    {
      feed_types.next();
      ostringstream oo;
      oo << "NoMDFeedTypes : market depth - " << feed_types.marketDepth();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnRefreshSecurityDefinitionSpread(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  MDInstrumentDefinitionSpread29 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noEvents();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo << "MDInstrumentDefinitionSpread29: "
        << " event time: " << SDateTime::fromUnixTimeToString(ent.eventTime(), SDateTime::NANOSEC)
        << " event type: " << ent.eventType()
        << " security ID: " << msg.securityID()
        << " symbol: " << msg.symbol()
        << " security group: " << msg.getSecurityGroupAsString()
        << " market segment ID: " << msg.marketSegmentID();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnQuoteRequest(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  QuoteRequest39 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& sym = msg.noRelatedSym();
  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (sym.hasNext())
    {
      sym.next();

      ostringstream oo;
      oo << "Quote Request: "
        << " symbol: " << sym.symbol()
        << " securityID: " << sym.securityID()
        << " OrderQty: " << sym.orderQty();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }
}

void DataProcFunctions_MDP::OnSecurityStatus(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  SecurityStatus30 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto security_status = msg.securityTradingStatus();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    {
      ostringstream oo;
      switch(security_status) {
        case SecurityTradingStatus::TradingHalt:            { oo << "Status: Trading Halt: ";              break; }
        case SecurityTradingStatus::Close:                  { oo << "Status: Close: ";                     break; }
        case SecurityTradingStatus::NewPriceIndication:     { oo << "Status: New Price Indication: ";      break; }
        case SecurityTradingStatus::ReadyToTrade:           { oo << "Status: Ready To Trade: ";            break; }
        case SecurityTradingStatus::NotAvailableForTrading: { oo << "Status: Not Available For Trading: "; break; }
        case SecurityTradingStatus::UnknownorInvalid:       { oo << "Status: Unknown or Invalid: ";        break; }
        case SecurityTradingStatus::PreOpen:                { oo << "Status: Pre-Open: ";                  break; }
        case SecurityTradingStatus::PreCross:               { oo << "Status: Pre-Cross: ";                 break; }
        case SecurityTradingStatus::Cross:                  { oo << "Status: Cross: ";                     break; }
        case SecurityTradingStatus::PostClose:              { oo << "Status: Post-Close: ";                break; }
        case SecurityTradingStatus::NoChange:               { oo << "Status: No Change: ";                 break; }
        case SecurityTradingStatus::NULL_VALUE:             { oo << "Status: NULL VALUE: ";                break; }
      }
      oo << security_status;
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }

    {
      ostringstream oo;
      oo << "SecurityStatus30: SecurityID: " << msg.securityID()
        << " SecurityGroup: " << msg.securityGroup()
        << " Asset: " << msg.asset()
        << " TradeDate: " << msg.tradeDate()
        << " SecurityTradingStatus: " << security_status
        << " HaltReason: " << msg.haltReason()
        << " SecurityTradingEvent: " << msg.securityTradingEvent();
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }


}

void DataProcFunctions_MDP::OnChannelReset(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  ChannelReset4 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    while (ent.hasNext())
    {
      ent.next();
      ostringstream oo;
      oo
        << "ChannelReset4: MDUpdateAction: " << (int)(ent.mDUpdateAction())
        << " MDEntryType: " << string(ent.mDEntryType());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }
  }

  //--------------------------------------------------
  // Reset Order Books of this particular channel
  //--------------------------------------------------
  SharedObjects::Instance()->ResetOrderBooksInChnl(channelID);

  set<unsigned long> * pset = SharedObjects::Instance()->GetOrderBookIDInChnl(channelID);
  vector<unsigned long> vOBID(pset->size());
  std::copy(pset->begin(), pset->end(), vOBID.begin());

  vector<string> vsOBID = FMap(vOBID, std::function<string(const unsigned long&)>([](const unsigned long& i) { return boost::lexical_cast<string>(i); }));
  const string sOBIDs = boost::algorithm::join(vsOBID, ",");
  m_Logger->Write(Logger::INFO,"DataProcFunctions_MDP: ChannelID:%u. %s Resetting OrderBooks upon reception of MDP_CHANNEL_RESET: %s", channelID, sMcastType.c_str(), sOBIDs.c_str());

}

void DataProcFunctions_MDP::OnSnapshotFullRefresh(const unsigned short channelID, const mktdata::MessageHeader& hdr, char *buf, const int len, const string & sMcastType)
{
  SnapshotFullRefresh38 msg;
  msg.wrapForDecode(buf, hdr.encodedLength(), hdr.blockLength(), MDP_VERSION, len);
  auto& ent = msg.noMDEntries();

  if (m_Logger->NeedToPrint(Logger::DEBUG))
  {
    {
      ostringstream oo;
      oo
        << "SnapshotFullRefresh38: SecurityID: " << (int)(msg.securityID())
        << " TotNumReport: " << (int)(msg.totNumReports())
        << " LastMsgSeqNumProcessed: " << (int)(msg.lastMsgSeqNumProcessed())
        << " RptSeq: " << (int)(msg.rptSeq())
        << " TradeDate: " << msg.tradeDate()
        << " SecurityTradingStatus: " << (int)(msg.mDSecurityTradingStatus())
        << " HighLimit: " << (int64_t)(msg.highLimitPrice().mantissa())
        << " LowLimit: " << (int64_t)(msg.lowLimitPrice().mantissa())
        << " MaxPriceVariation: " << (int64_t)(msg.maxPriceVariation().mantissa());
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());
    }

    while (ent.hasNext())
    {
      ent.next();

      ostringstream oo;
      oo
        << "SnapshotFullRefresh38: SecurityID: " << (int)(msg.securityID())
        << " Price: " << (int64_t)(ent.mDEntryPx().mantissa())
        << " EntrySize: " << (int)(ent.mDEntrySize())
        << " OrderNo: " << (int)(ent.numberOfOrders())
        << " MDPriceLevel: " << (int)(ent.mDPriceLevel())
        << " tradingReferenceDate: " << ent.tradingReferenceDate()
        << " OpenCloseSettlFlag: " << (int)(ent.openCloseSettlFlag())
        << " EntryType: " << (char)(ent.mDEntryType());

      oo << " Settlement Price Type: ";
      if (ent.settlPriceType().finalfinal())   oo << "finalfinal ";
      if (ent.settlPriceType().actual())       oo << "actual ";
      if (ent.settlPriceType().rounded())      oo << "rounded ";
      if (ent.settlPriceType().intraday())     oo << "intraday ";
      if (ent.settlPriceType().reservedBits()) oo << "reservedBits ";
      if (ent.settlPriceType().nullValue())    oo << "null ";
      m_Logger->Write(Logger::DEBUG,"DataProcFunctions_MDP: ChannelID:%u. %s %s", channelID, sMcastType.c_str(), oo.str().c_str());

    }
  }

}

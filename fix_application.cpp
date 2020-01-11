#include "fix_application.h"

// Returns SessionID - MarketData for md = true, Trading fr md = false
SessionID FixApplication::sessionID(bool md)
{
	for (auto session : sessions)
	{
		// for FXCM MarketData sessions begin with MD_
		if ((session.toString().find("MD_") != string::npos) == md)
		{
			return session;
		}
	}
	return SessionID();
}

FixApplication::FixApplication()
{
	// Initialize unsigned int requestID to 1. We will use this as a 
	// counter for making request IDs
	requestID = 1;
}

// Gets called when quickfix creates a new session. A session comes into and remains in existence
// for the life of the application.
void FixApplication::onCreate(const SessionID& session_ID)
{
	// FIX Session created. We must now logon. QuickFIX will automatically send
	// the Logon(A) message
	cout << "Session -> created" << session_ID << endl;
	sessions.push_back(session_ID);
}

// Notifies you when a valid logon has been established with FXCM.
void FixApplication::onLogon(const SessionID& session_ID)
{
	// Session logon successful. Now we request TradingSessionStatus which is
	// used to determine market status (open or closed), to get a list of securities,
	// and to obtain important FXCM system parameters 
	cout << "Session -> logon" << session_ID << endl;
	GetTradingStatus();
}

// Notifies you when an FIX session is no longer online. This could happen during a normal logout
// exchange or because of a forced termination or a loss of network connection. 
void FixApplication::onLogout(const SessionID& session_ID)
{
	// Session logout 
	cout << "Session -> logout" << session_ID << endl;
}

// Provides you with a peak at the administrative messages that are being sent from your FIX engine 
// to FXCM.
void FixApplication::toAdmin(Message& message, const SessionID& session_ID)
{
	// If the Admin message being sent to FXCM is of typle Logon (A), we want
	// to set the Username and Password fields. We want to catch this message as it
	// is going out.
	string msg_type = message.getHeader().getField(FIELD::MsgType);
	if(msg_type == "A"){
		// Get both username and password from our settings file. Then set these
		// respective fields
		string user = settings->get().getString("Username");
		string pass = settings->get().getString("Password");
		message.setField(Username(user));
		message.setField(Password(pass));
	}
	// All messages sent to FXCM must contain the TargetSubID field (both Administrative and
	// Application messages). Here we set this.
	string sub_ID = settings->get(session_ID).getString("TargetSubID");
	message.getHeader().setField(TargetSubID(sub_ID));
}

// A callback for application messages that you are being sent to a counterparty. 
void FixApplication::toApp(Message& message, const SessionID& session_ID)
{
	// All messages sent to FXCM must contain the TargetSubID field (both Administrative and
	// Application messages). Here we set this.
	string sub_ID = settings->get(session_ID).getString("TargetSubID");
	message.getHeader().setField(TargetSubID(sub_ID));
}

// Notifies you when an administrative message is sent from FXCM to your FIX engine. 
void FixApplication::fromAdmin(const Message& message, const SessionID& session_ID)
{
	// Call MessageCracker.crack method to handle the message by one of our 
	// overloaded onMessage methods below
	crack(message, session_ID);
}

// One of the core entry points for your FIX application. Every application level request will come through here. 
void FixApplication::fromApp(const Message& message, const SessionID& session_ID)
{
	// Call MessageCracker.crack method to handle the message by one of our 
	// overloaded onMessage methods below
	crack(message, session_ID);
}

// The TradingSessionStatus message is used to provide an update on the status of the market. Furthermore, 
// this message contains useful system parameters as well as information about each trading security (embedded SecurityList).
// TradingSessionStatus should be requested upon successful Logon and subscribed to. The contents of the
// TradingSessionStatus message, specifically the SecurityList and system parameters, should dictate how fields
// are set when sending messages to FXCM.
void FixApplication::onMessage(const FIX44::TradingSessionStatus& tss, const SessionID& session_ID)
{
	// Check TradSesStatus field to see if the trading desk is open or closed
	// 2 = Open; 3 = Closed
	string trad_status = tss.getField(FIELD::TradSesStatus);
	cout << "TradingSessionStatus -> TradSesStatus -" << trad_status << endl;
	// Within the TradingSessionStatus message is an embeded SecurityList. From SecurityList we can see
	// the list of available trading securities and information relevant to each; e.g., point sizes,
	// minimum and maximum order quantities by security, etc. 
	cout << "  SecurityList via TradingSessionStatus -> " << endl;
	int symbols_count = IntConvertor::convert(tss.getField(FIELD::NoRelatedSym));
	for(int i = 1; i <= symbols_count; i++){
		// Get the NoRelatedSym group and for each, print out the Symbol value
		FIX44::SecurityList::NoRelatedSym symbols_group;
		tss.getGroup(i,symbols_group);
		string symbol = symbols_group.getField(FIELD::Symbol);
		cout << "    Symbol -> " << symbol << endl;
	}
	// Also within TradingSessionStatus are FXCM system parameters. This includes important information
	// such as account base currency, server time zone, the time at which the trading day ends, and more.
	cout << "  System Parameters via TradingSessionStatus -> " << endl;
	// Read field FXCMNoParam (9016) which shows us how many system parameters are 
	// in the message
	int params_count = IntConvertor::convert(tss.getField(FXCM_NO_PARAMS)); // FXCMNoParam (9016)
	for(int i = 1; i < params_count; i++){
		// For each paramater, print out both the name of the paramater and the value of the 
		// paramater. FXCMParamName (9017) is the name of the paramater and FXCMParamValue(9018)
		// is of course the paramater value
		FIX::FieldMap field_map = tss.getGroupRef(i,FXCM_NO_PARAMS);
		cout << "    Param Name -> " << field_map.getField(FXCM_PARAM_NAME) 
			<< " - Param Value -> " << field_map.getField(FXCM_PARAM_VALUE) << endl;
	}
	// Request accounts under our login
	GetAccounts();

	// ** Note on Text(58) ** 
	// You will notice that Text(58) field is always set to "Market is closed. Any trading
	// functionality is not available." This field is always set to this value; therefore, do not 
	// use this field value to determine if the trading desk is open. As stated above, use TradSesStatus for this purpose
}

void FixApplication::onMessage(const FIX44::CollateralInquiryAck& ack, const SessionID& session_ID)
{

}

// CollateralReport is a message containing important information for each account under the login. It is returned
// as a response to CollateralInquiry. You will receive a CollateralReport for each account under your login.
// Notable fields include Account(1) which is the AccountID and CashOutstanding(901) which is the account balance
void FixApplication::onMessage(const FIX44::CollateralReport& cr, const SessionID& session_ID)
{
	cout << "CollateralReport -> " << endl;
	string accountID = cr.getField(FIELD::Account);
	// Get account balance, which is the cash balance in the account, not including any profit
	// or losses on open trades
	string balance = cr.getField(FIELD::CashOutstanding);
	cout << "  AccountID -> " << accountID << endl;
	cout << "  Balance -> " << balance << endl;
	// The CollateralReport NoPartyIDs group can be inspected for additional account information
	// such as AccountName or HedgingStatus
	FIX44::CollateralReport::NoPartyIDs group;
	cr.getGroup(1,group); // CollateralReport will only have 1 NoPartyIDs group
	cout << "  Parties -> "<< endl;
	// Get the number of NoPartySubIDs repeating groups
	int number_subID = IntConvertor::convert(group.getField(FIELD::NoPartySubIDs));
	// For each group, print out both the PartySubIDType and the PartySubID (the value)
	for(int u = 1; u <= number_subID; u++){
		FIX44::CollateralReport::NoPartyIDs::NoPartySubIDs sub_group;
		group.getGroup(u,sub_group);

		string sub_type = sub_group.getField(FIELD::PartySubIDType);
		string sub_value = sub_group.getField(FIELD::PartySubID);
		cout << "    " << sub_type << " -> " << sub_value << endl;
	}
	// Add the accountID to our vector<string> being used to track all
	// accounts under our login
	RecordAccount(accountID);
}

void FixApplication::onMessage(const FIX44::RequestForPositionsAck& ack, const SessionID& session_ID)
{
	string pos_reqID = ack.getField(FIELD::PosReqID);
	cout << "RequestForPositionsAck -> PosReqID - " << pos_reqID << endl;

	// If a PositionReport is requested and no positions exist for that request, the Text field will
	// indicate that no positions mathced the requested criteria 
	if(ack.isSetField(FIELD::Text))
		cout << "RequestForPositionsAck -> Text - " << ack.getField(FIELD::Text) << endl;
}

void FixApplication::onMessage(const FIX44::PositionReport& pr, const SessionID& session_ID)
{
	// Print out important position related information such as accountID and symbol 
	string accountID = pr.getField(FIELD::Account);
	string symbol = pr.getField(FIELD::Symbol);
	string positionID = pr.getField(FXCM_POS_ID);
	string pos_open_time = pr.getField(FXCM_POS_OPEN_TIME);
	cout << "PositionReport -> " << endl;
	cout << "   Account -> " << accountID << endl;
	cout << "   Symbol -> " << symbol << endl;
	cout << "   PositionID -> " << positionID << endl;
	cout << "   Open Time -> " << pos_open_time << endl;
}

void FixApplication::onMessage(const FIX44::MarketDataRequestReject& mdr, const SessionID& session_ID)
{
	// If MarketDataRequestReject is returned as the result of a MarketDataRequest message,
	// print out the contents of the Text field but first check that it is set
	cout << "MarketDataRequestReject -> " << endl;
	if(mdr.isSetField(FIELD::Text)){
		cout << " Text -> " << mdr.getField(FIELD::Text) << endl;
	}
}

void FixApplication::onMessage(const FIX44::MarketDataSnapshotFullRefresh& mds, const SessionID& session_ID)
{
	// Get symbol name of the snapshot; e.g., EUR/USD. Our example only subscribes to EUR/USD so 
	// this is the only possible value
	string symbol = mds.getField(FIELD::Symbol);
	// Declare variables for both the bid and ask prices. We will read the MarketDataSnapshotFullRefresh
	// message for tthese values
	double bid_price = 0;
	double ask_price = 0;
	// For each MDEntry in the message, inspect the NoMDEntries group for
	// the presence of either the Bid or Ask (Offer) type 
	int entry_count = IntConvertor::convert(mds.getField(FIELD::NoMDEntries));
	for(int i = 1; i < entry_count; i++){
		FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
		mds.getGroup(i,group);
		string entry_type = group.getField(FIELD::MDEntryType);
		if(entry_type == "0"){ // Bid
			bid_price = DoubleConvertor::convert(group.getField(FIELD::MDEntryPx));
		}else if(entry_type == "1"){ // Ask (Offer)
			ask_price = DoubleConvertor::convert(group.getField(FIELD::MDEntryPx));
		}
	}
	cout << "MarketDataSnapshotFullRefresh -> Symbol - " << symbol 
		<< " Bid - " << bid_price << " Ask - " << ask_price << endl; 
}

void FixApplication::onMessage(const FIX44::ExecutionReport& er, const SessionID& session_ID)
{
	cout << "ExecutionReport -> " << endl;
	cout << "  ClOrdID -> " << er.getField(FIELD::ClOrdID) << endl; 
	cout << "  Account -> " << er.getField(FIELD::Account) << endl;
	cout << "  OrderID -> " << er.getField(FIELD::OrderID) << endl;
	cout << "  LastQty -> " << er.getField(FIELD::LastQty) << endl;
	cout << "  CumQty -> " << er.getField(FIELD::CumQty) << endl;
	cout << "  ExecType -> " << er.getField(FIELD::ExecType) << endl;
	cout << "  OrdStatus -> " << er.getField(FIELD::OrdStatus) << endl;

	// ** Note on order status. ** 
	// In order to determine the status of an order, and also how much an order is filled, we must
	// use the OrdStatus and CumQty fields. There are 3 possible final values for OrdStatus: Filled (2),
	// Rejected (8), and Cancelled (4). When the OrdStatus field is set to one of these values, you know
	// the execution is completed. At this time the CumQty (14) can be inspected to determine if and how
	// much of an order was filled.
}

// Starts the FIX session. Throws FIX::ConfigError exception if our configuration settings
// do not pass validation required to construct SessionSettings 
void FixApplication::StartSession()
{
	try{
		settings      = new SessionSettings("settings.cfg");
		store_factory = new FileStoreFactory(* settings);
		log_factory   = new FileLogFactory(* settings);
		initiator     = new SocketInitiator(* this, * store_factory, * settings, * log_factory/*Optional*/);
		initiator->start();
	}catch(ConfigError error){
		cout << error.what() << endl;
	}
}

// Logout and end session 
void FixApplication::EndSession()
{
	initiator->stop();
	delete initiator;
	delete settings;
	delete store_factory;
	delete log_factory;
}

// Sends TradingSessionStatusRequest message in order to receive as a response the
// TradingSessionStatus message
void FixApplication::GetTradingStatus()
{
	// Request TradingSessionStatus message 
	FIX44::TradingSessionStatusRequest request;
	request.setField(TradSesReqID(NextRequestID()));
	request.setField(TradingSessionID("FXCM"));
	request.setField(SubscriptionRequestType(SubscriptionRequestType_SNAPSHOT));
	Session::sendToTarget(request, sessionID(false));
}

// Sends the CollateralInquiry message in order to receive as a response the
// CollateralReport message.
void FixApplication::GetAccounts()
{
	// Request CollateralReport message. We will receive a CollateralReport for each
	// account under our login
	FIX44::CollateralInquiry request;
	request.setField(CollInquiryID(NextRequestID()));
	request.setField(TradingSessionID("FXCM"));
	request.setField(SubscriptionRequestType(SubscriptionRequestType_SNAPSHOT));
	Session::sendToTarget(request, sessionID(false));
}

// Sends RequestForPositions which will return PositionReport messages if positions
// matching the requested criteria exist; otherwise, a RequestForPositionsAck will be
// sent with the acknowledgement that no positions exist. In our example, we request
// positions for all accounts under our login
void FixApplication::GetPositions()
{
	// Here we will get positions for each account under our login. To do this,
	// we will send a RequestForPositions message that contains the accountID 
	// associated with our request. For each account in our list, we send
	// RequestForPositions. 
	int total_accounts = (int)list_accountID.size();
	for(int i = 0; i < total_accounts; i++){
		string accountID = list_accountID.at(i);
		// Set default fields
		FIX44::RequestForPositions request;
		request.setField(PosReqID(NextRequestID()));
		request.setField(PosReqType(PosReqType_POSITIONS));
		// AccountID for the request. This must be set for routing purposes. We must
		// also set the Parties AccountID field in the NoPartySubIDs group
		request.setField(Account(accountID)); 
		request.setField(SubscriptionRequestType(SubscriptionRequestType_SNAPSHOT));
		request.setField(AccountType(
			AccountType_ACCOUNT_IS_CARRIED_ON_NON_CUSTOMER_SIDE_OF_BOOKS_AND_IS_CROSS_MARGINED));
		request.setField(TransactTime());
		request.setField(ClearingBusinessDate());
		request.setField(TradingSessionID("FXCM"));
		// Set NoPartyIDs group. These values are always as seen below
		request.setField(NoPartyIDs(1));
		FIX44::RequestForPositions::NoPartyIDs parties_group;
		parties_group.setField(PartyID("FXCM ID"));
		parties_group.setField(PartyIDSource('D'));
		parties_group.setField(PartyRole(3));
		parties_group.setField(NoPartySubIDs(1));
		// Set NoPartySubIDs group
		FIX44::RequestForPositions::NoPartyIDs::NoPartySubIDs sub_parties;
		sub_parties.setField(PartySubIDType(PartySubIDType_SECURITIES_ACCOUNT_NUMBER));
		// Set Parties AccountID
		sub_parties.setField(PartySubID(accountID));
		// Add NoPartySubIds group
		parties_group.addGroup(sub_parties);
		// Add NoPartyIDs group
		request.addGroup(parties_group);
		// Send request
		Session::sendToTarget(request, sessionID(false));
	}
}

// Subscribes to the EUR/USD trading security
void FixApplication::SubscribeMarketData(string strPair)
{
	// Subscribe to market data for EUR/USD
	string request_ID = strPair + "_Request_";
	FIX44::MarketDataRequest request;
	request.setField(MDReqID(request_ID));
	request.setField(SubscriptionRequestType(
		SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES));
	request.setField(MarketDepth(0));
	request.setField(NoRelatedSym(1));

	// Add the NoRelatedSym group to the request with Symbol
	// field set to EUR/USD
	FIX44::MarketDataRequest::NoRelatedSym symbols_group;
	symbols_group.setField(Symbol(strPair));
	request.addGroup(symbols_group);

	// Add the NoMDEntryTypes group to the request for each MDEntryType
	// that we are subscribing to. This includes Bid, Offer, High, and Low
	FIX44::MarketDataRequest::NoMDEntryTypes entry_types;
	entry_types.setField(MDEntryType(MDEntryType_BID));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_OFFER));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_TRADING_SESSION_HIGH_PRICE));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_TRADING_SESSION_LOW_PRICE));
	request.addGroup(entry_types);

	Session::sendToTarget(request, sessionID(true));
}

// Unsubscribes from the EUR/USD trading security 
void FixApplication::UnsubscribeMarketData()
{
	// Unsubscribe from EUR/USD. Note that our request_ID is the exact same
	// that was sent for our request to subscribe. This is necessary to 
	// unsubscribe. This request below is identical to our request to subscribe
	// with the exception that SubscriptionRequestType is set to
	// "SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT_PLUS_UPDATE_REQUEST"
	string request_ID = "EUR_USD_Request_";
	FIX44::MarketDataRequest request;
	request.setField(MDReqID(request_ID));
	request.setField(SubscriptionRequestType(
		SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT_PLUS_UPDATE_REQUEST));
	request.setField(MarketDepth(0));
	request.setField(NoRelatedSym(1));

	// Add the NoRelatedSym group to the request with Symbol
	// field set to EUR/USD
	FIX44::MarketDataRequest::NoRelatedSym symbols_group;
	symbols_group.setField(Symbol("EUR/USD"));
	request.addGroup(symbols_group);

	// Add the NoMDEntryTypes group to the request for each MDEntryType
	// that we are subscribing to. This includes Bid, Offer, High, and Low
	FIX44::MarketDataRequest::NoMDEntryTypes entry_types;
	entry_types.setField(MDEntryType(MDEntryType_BID));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_OFFER));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_TRADING_SESSION_HIGH_PRICE));
	request.addGroup(entry_types);
	entry_types.setField(MDEntryType(MDEntryType_TRADING_SESSION_LOW_PRICE));
	request.addGroup(entry_types);

	Session::sendToTarget(request, sessionID(true));
}

// Sends a basic NewOrderSingle message to buy EUR/USD at the 
// current market price
void FixApplication::MarketOrder()
{
	// For each account in our list, send a NewOrderSingle message
	// to buy EUR/USD. What differentiates this message is the
	// accountID
	int total_accounts = (int)list_accountID.size();
	for(int i = 0; i < total_accounts; i++){
		string accountID = list_accountID.at(i);
		FIX44::NewOrderSingle request;
		request.setField(ClOrdID(NextRequestID()));	
		request.setField(Account(accountID));
		request.setField(Symbol("EUR/USD"));
		request.setField(TradingSessionID("FXCM"));
		request.setField(TransactTime());
		request.setField(OrderQty(10000));
		request.setField(Side(FIX::Side_BUY));
		request.setField(OrdType(OrdType_MARKET));
		request.setField(TimeInForce(FIX::TimeInForce_GOOD_TILL_CANCEL)); // For newer versions of QuickFIX change this to TimeInForce_GOOD_TILL_CANCEL
		Session::sendToTarget(request, sessionID(false));
	}
}

// Generate string value used to populate the fields in each message
// which are used as a custom identifier
string FixApplication::NextRequestID()
{
	if(requestID == 65535)
		requestID = 1;

	requestID++;
	string next_ID = IntConvertor::convert(requestID);
	return next_ID;
}

// Adds string accountIDs to our vector<string> being used to
// account for the accountIDs under our login
void FixApplication::RecordAccount(string accountID)
{
	int size = (int)list_accountID.size();
	if(size == 0){
		list_accountID.push_back(accountID);
	}else{
		for(int i = 0; i < size; i++){
			if(list_accountID.at(i) == accountID)
				break;
			if(i == size - 1){
				list_accountID.push_back(accountID);
			}
		}
	}
}
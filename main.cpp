#include "fix_application.h"

// -- FIX Example --
//
// Upon starting this application, a FIX session will be created and the connection
// sequence will commence. This includes sending a Logon message, a request for
// TradingSessionStatus, and a request to get accounts (CollateralInquiry). After 
// the responses to these requests are received, you can use the command prompt
// to test out the functionality seen below in the switch block.
//
// --

int main()
{
	FixApplication app;
	// Start session and Logon
	app.StartSession();

	while(true){
		int command = 0;
		bool exit = false;
		cin >> command;

		switch(command){
		case 0: // Exit example application
			exit = true;
			break;
		case 1: // Get positions 
			app.GetPositions();
			break;
		case 2: // Subscribe to market data
			app.SubscribeMarketData("EUR/USD");
			app.SubscribeMarketData("EUR/JPY");
			app.SubscribeMarketData("EUR/GBP");
			break;
		case 3: // Unsubscribe to market data
			app.UnsubscribeMarketData();
			break;
		case 4: // Send market order
			app.MarketOrder();
			break;
		}
		if(exit)
			break;
	}

	// End session and logout
	app.EndSession();
	//while(true){
	//} // Wait
	return 0;
}
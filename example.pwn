#include <a_samp>
#include "translate"

public OnFilterScriptInit() {
	// Do this once for each language
	new spanish = LoadLanguage("spanish.lang.inc");
	
	if (spanish == -1) {
		print("Unable to load spanish.lang.inc, make sure it's in your scriptfiles folder!");
		return 1;
	}
	
	// test playerid
	new playerid = 0;
	
	// set language for playerid 0
	SetPlayerLanguage(0, spanish);
	
	// send messages in playerid's language
	print(__("hello mr world", playerid));
	print(__("Come on, hurry up!", playerid));
	print(__("How are you?", playerid));
	print(__("I don't like green bananas", playerid));
	
	return 1;
}

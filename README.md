samp-translate
==============

Translate your scripts easily!

## Getting started

1. Get [amx_assembly](https://github.com/Zeex/amx_assembly) and [strlib](https://github.com/oscar-broman/strlib).
2. `#include <translate>`

## Usage

1. Create the translation file in the `scriptfiles` directory.
2. Load the language:  
`g_LangSwedish = LoadLanguage("swedish.lang.inc");`
3. Set a player's language:  
`SetPlayerLanguage(playerid, g_LangSwedish);`
4. Send translated messages to the player:  
`SendClientMessage(playerid, color, __("hello!", playerid));`
5. Run the server to automatically fill the translation file after you used `__`.

## Translation files

The translation files look like this:
```
"original text 1" = "translated text 1"
"original text 2" = "translated text 2"
"original text 3" = "translated text 3"
```

## Planned features

* Make a `_("string")` macro that doesn't require a `playerid` every time. Automatically figure out the current player by setting it in hooked callbacks, `SendClientMessage`, etc.
* [Your suggestion](https://github.com/oscar-broman/samp-translate/issues/new)!

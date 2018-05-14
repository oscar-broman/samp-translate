#include "a_samp"
#tryinclude "strlib"
#tryinclude "amx_assembly\opcode"
#tryinclude "amx_assembly\phys_memory"
#tryinclude "amx_assembly\asm"

#if !defined STRLIB_INC
	#error The strlib library is required, get it here: github.com/oscar-broman/strlib
#endif

// Make sure amx_assembly was included
#if !defined OPCODE_INC
	#error The amx_assembly library is required, get it here: github.com/Zeex/amx_assembly
#endif

// Make sure it's a recent version
#if tagof(NUM_OPCODES) == tagof(OP_NONE)
	#error Please get the latest version of the amx_assembly files: github.com/Zeex/amx_assembly
#endif

#if !defined MAX_TRANSLATE_STRINGS
	#define MAX_TRANSLATE_STRINGS 512
#endif

#if !defined MAX_TRANSLATE_LANGUAGES
	#define MAX_TRANSLATE_LANGUAGES 4
#endif

#if !defined MAX_TRANSLATE_DATA
	#define MAX_TRANSLATE_DATA (MAX_TRANSLATE_LANGUAGES * MAX_TRANSLATE_STRINGS * 128)
#endif

// Number of operands an opcode takes
static const sc_OpcodeOperands[Opcode:138] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
	1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,
	1, 0, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, 0, 1, -1, 0, 0, 1, 0, 1,
	1, 0
};

stock const TRANSLATE_MAGIC = 0xD253E095;

stock
	// Translate strings
	g_TS[MAX_TRANSLATE_STRINGS][MAX_TRANSLATE_LANGUAGES][1],
	// Current language
	#define g_PlayerLanguage g_PL
	g_PL[MAX_PLAYERS]
;

static stock
	// Translate data (translated strings are dumped in here)
	gs_TranslateData[MAX_TRANSLATE_DATA],
	gs_TranslateDataLength = 0,
	// Upper language index
	gs_TsLangIdx = 1,
	// Upper index in g_TS
	gs_TsIdx = 0
;

// Read and relocate an opcode
static stock Opcode:ReadOpcode(address);
// Read the operand for the current opcode, assuming it has one
static stock ReadOperand(address, end = 0);
// Write an opcode and its operand (if it has one)
static stock WriteOpcode(address, Opcode:opcode, operand = 0);
// Write an operand
static stock WriteOperand(address, operand);
// Find and store the next opcode
static stock FindNextOpcode(&address, &Opcode:opcode, end = 0);
// Find a specific opcode
// Modifies address only on success
static stock FindOpcode(&address, Opcode:opcode, end = 0);
// Find a specific opcode and operand pair
// Modifies address only on success
static stock FindOpcodeWithOperand(&address, Opcode:opcode, operand, end = 0);
// Change the address of a dimension in an array. The 2nd last dimension will be modified. 
// RedirectArraySlot(array[][], slot, address)
static stock RedirectArraySlot(...);

// TODO:
#define _(%1) \
	g_TS[GetTS("  " #%1)][g_CI]

#define __(%1,%2) \
	g_TS[GetTS("  " #%1)][g_PL[%2]]

#define ___(%1,%2) \
	g_TS[GetTS("  " #%1)][%2]

stock GetTS(...) {
	return 0;
}

static Init() {
	static s_initialized = false;
	
	if (s_initialized) {
		return;
	} else {
		s_initialized = true;
	}
	
	// Gather some info about the AMX
	new hdr[AMX_HDR], base, cod, dat;

	GetAmxHeader(hdr);
	
	base = GetAmxBaseAddress();
	cod = hdr[AMX_HDR_COD];
	dat = hdr[AMX_HDR_DAT];

	new cod_start = cod - dat;
	new address = cod_start;
	
	// Get the addresses of some functions we'll need
	new addr_GetTS;

	#emit CONST.pri   GetTS
	#emit STOR.S.pri  addr_GetTS

	// Relocate them (so they can be used for AMX instructions)
	addr_GetTS += base + cod;

	// Look for GetTS invocations
	while (FindOpcodeWithOperand(address, OP_CALL, addr_GetTS)) {
		new str_addr;
		
		switch (ReadOpcode(address - 20)) {
			case OP_PUSH_PRI: {
				str_addr = ReadOperand(address - 16);
			}
			
			case OP_CONST_PRI: {
				str_addr = ReadOperand(address - 20);
				
				WriteOpcode(address - 20, OP_NOP);
			}
			
			default: {
				continue;
			}
		}
		
		new idx = -1;
		new tmp[1][1];
		
		// tmp[0] -> @"str"
		RedirectArraySlot(tmp, 0, str_addr + 8);
		
		// Search for a duplicate
		for (new i = 0; i < gs_TsIdx; i++) {
			if (isequal(g_TS[i][0], tmp[0])) {
				idx = i;
				break;
			}
		}
		
		if (idx == -1) {
			idx = gs_TsIdx++;
		}
		
		if (idx >= MAX_TRANSLATE_STRINGS) {
			print("ERROR: Out of space. Increase MAX_TRANSLATE_STRINGS.");
			
			exit;
		}
		
		// g_TS[GetTS("str")] -> g_TS[idx]
		WriteOpcode(address - 16, OP_NOP);
		WriteOpcode(address - 12, OP_NOP);
		WriteOpcode(address -  8, OP_NOP);
		WriteOpcode(address -  4, OP_NOP);
		WriteOpcode(address     , OP_CONST_PRI, idx);
		
		// Save information before the string (first two spaces added by the macro)
		// Write the magic so we can identify if a string is from this script
		WriteAmxMemory(str_addr    , TRANSLATE_MAGIC);
		// Lower 2 bytes is the index in g_TS and upper 2 bytes is the language index
		// The default language index is 0 so we don't need to write it
		WriteAmxMemory(str_addr + 4, idx);
		
		// g_TS[idx][0] = @"str"
		RedirectArraySlot(g_TS[idx], 0, str_addr + 8);
	}
}

stock LoadLanguage(const fname[], langidx = -1) {
	if (!fexist(fname)) {
		return -1;
	}
	
	new File:fp = fopen(fname, io_read);
	
	if (!fp) {
		return -1;
	}
	
	new buf[1024];
	new found[MAX_TRANSLATE_STRINGS] = {false, ...};
	
	if (langidx == -1) {
		langidx = gs_TsLangIdx++;
	}
	
	while (fread(fp, buf)) {
		strtrim(buf);
		
		// Invalid line?
		if (buf[0] != '"') {
			continue;
		}
		
		new original[512], translation[512], pos = 0;
		
		// Read the original text
		if (!strfromliteral(original, buf, pos)) {
			goto invalid_line;
		}
		
		// Skip whitespace and the equality sign
		while (buf[pos] <= ' ' || buf[pos] == '=') {
			pos++;
		}
		
		if (buf[pos] != '"') {
			goto invalid_line;
		}
		
		// Read the translation
		if (!strfromliteral(translation, buf, pos)) {
			goto invalid_line;
		}
		
		for (new i = 0; i < gs_TsIdx; i++) {
			if (!isequal(g_TS[i][0], original)) {
				continue;
			}
			
			found[i] = true;
			
			// See Init function for details
			WriteAmxMemory(ref(gs_TranslateData[gs_TranslateDataLength]), TRANSLATE_MAGIC);
			WriteAmxMemory(ref(gs_TranslateData[gs_TranslateDataLength + 1]), langidx << 16 | i);
			
			gs_TranslateDataLength += 2;
			
			strcat(gs_TranslateData[gs_TranslateDataLength], translation, sizeof(gs_TranslateData) - gs_TranslateDataLength);
			
			// Redirect g_TS[i][langidx] to gs_TranslateData[gs_TranslateDataLength]
			RedirectArraySlot(g_TS[i], langidx, ref(gs_TranslateData[gs_TranslateDataLength]));
			
			gs_TranslateDataLength += strlen(translation) + 1;
		}
		
		goto ok;
		
invalid_line:
		printf("(translate.inc) Invalid line: %s", buf);
ok:
	}
	
	fclose(fp);
	
	fp = File:0;
	
	new count = 0;
	
	for (new i = 0; i < gs_TsIdx; i++) {
		if (found[i]) {
			continue;
		}
		
		count++;
		
		if (!fp) {
			fp = fopen(fname, io_append);
		}
		
		buf = "\n";
		
		// "str" = "str"
		strtoliteral(buf[1], g_TS[i][0], sizeof(buf) - 1, false);
		strcat(buf, " = ");
		new len = strlen(buf);
		strtoliteral(buf[len], g_TS[i][0], sizeof(buf) - len, false);
		fwrite(fp, buf);
		
		// Redirect g_TS[i][langidx] to g_TS[i][0]
		RedirectArraySlot(g_TS[i], langidx, ref(g_TS[i][0]));
	}
	
	if (count) {
		printf("(translate.inc) Added %d strings to %s", count, fname);
	}
	
	return langidx;
}

stock SetPlayerLanguage(playerid, lang) {
	if (0 <= lang < gs_TsLangIdx) {
		g_PlayerLanguage[playerid] = lang;
		
		return true;
	}
	
	return false;
}

stock GetPlayerLanguage(playerid) {
	return g_PlayerLanguage[playerid];
}

stock IsLanguageString(const string[]) {
	new addr = ref(string);
	
	if (addr >= 8 && ReadAmxMemory(addr - 8) == TRANSLATE_MAGIC) {
		return true;
	}
	
	return false;
}

stock GetLanguageStringInfo(const string[], &langidx, &idx) {
	new addr = ref(string);
	
	if (addr >= 8 && ReadAmxMemory(addr - 8) == TRANSLATE_MAGIC) {
		langidx = ReadAmxMemory(addr - 4) >>> 16;
		idx = ReadAmxMemory(addr - 4) & 0xFFFF;
	
		if (0 <= langidx < MAX_TRANSLATE_LANGUAGES && 0 <= idx < MAX_TRANSLATE_STRINGS) {
			return true;
		}
	}
	
	return false;
}

// Hook OnGameModeInit
public OnGameModeInit() {
	Init();
	
	#if defined TRA_OnGameModeInit
		TRA_OnGameModeInit();
	#else
		return 1;
	#endif
}

#if defined _ALS_OnGameModeInit
	#undef OnGameModeInit
#else
	#define _ALS_OnGameModeInit
#endif

#define OnGameModeInit TRA_OnGameModeInit

#if defined TRA_OnGameModeInit
	forward TRA_OnGameModeInit();
#endif

// Hook OnJITCompile
forward OnJITCompile();
public OnJITCompile() {
	Init();
	
	#if defined TRA_OnJITCompile
		TRA_OnJITCompile();
	#else
		return 1;
	#endif
}

#if defined _ALS_OnJITCompile
	#undef OnJITCompile
#else
	#define _ALS_OnJITCompile
#endif

#define OnJITCompile TRA_OnJITCompile

#if defined TRA_OnJITCompile
	forward TRA_OnJITCompile();
#endif

// Hook OnFilterScriptInit
public OnFilterScriptInit() {
	Init();
	
	#if defined TRA_OnFilterScriptInit
		return TRA_OnFilterScriptInit();
	#else
		return 1;
	#endif
}

#if defined _ALS_OnFilterScriptInit
	#undef OnFilterScriptInit
#else
	#define _ALS_OnFilterScriptInit
#endif

#define OnFilterScriptInit TRA_OnFilterScriptInit

#if defined TRA_OnFilterScriptInit
	forward TRA_OnFilterScriptInit();
#endif

// Utilities
static stock Opcode:ReadOpcode(address) {
	new Opcode:opcode = Opcode:ReadAmxMemory(address);
	opcode = UnrelocateOpcode(opcode);

	// Make sure it's valid
	assert _:OP_NONE <= _:opcode < NUM_OPCODES;

	return opcode;
}

static stock ReadOperand(address, end = 0) {
	assert sc_OpcodeOperands[ReadOpcode(address)] == 1;
	assert address + 4 < end;

	return ReadAmxMemory(address + 4);
}

static stock WriteOpcode(address, Opcode:opcode, operand = 0) {
	// Are we dealing with a valid opcode?
	assert _:OP_NONE <= _:opcode < NUM_OPCODES;
	assert sc_OpcodeOperands[opcode] != -1;

	new Opcode:opcode_reloc = RelocateOpcode(opcode);

	// There's a bug in an earlier version causing issues on Windows
	if (opcode && !opcode_reloc) {
		print(!"\nERROR: Please get the latest version of the amx_assembly files: github.com/Zeex/amx_assembly\n");

		exit;
	}

	WriteAmxMemory(address, _:opcode_reloc);

	if (sc_OpcodeOperands[opcode]) {
		WriteAmxMemory(address + 4, operand);
	}
}

static stock WriteOperand(address, operand) {
	assert sc_OpcodeOperands[ReadOpcode(address)] == 1;

	WriteAmxMemory(address + 4, operand);
}

static stock FindNextOpcode(&address, &Opcode:opcode, end = 0) {
	new Opcode:current_opcode = ReadOpcode(address);
	new start_address = address;

	address += 4;

	if (address >= end) {
		address = start_address;

		return 0;
	}

	// Case tables have dynamic operand sizes
	if (current_opcode == OP_CASETBL) {
		address += 8 + 8 * ReadAmxMemory(address);
	} else {
		// Are we dealing with a valid opcode?
		assert sc_OpcodeOperands[current_opcode] != -1;

		address += 4 * sc_OpcodeOperands[current_opcode];
	}

	// Nothing more to read
	if (address >= end) {
		address = start_address;

		return 0;
	}

	opcode = ReadOpcode(address);

	return 1;
}

static stock FindOpcode(&address, Opcode:opcode, end = 0) {
	new start_address = address;
	new Opcode:current_opcode;

	while (FindNextOpcode(address, current_opcode, end)) {
		if (current_opcode == opcode) {
			return 1;
		}
	}

	// Nothing was found
	address = start_address;

	return 0;
}

static stock FindOpcodeWithOperand(&address, Opcode:opcode, operand, end = 0) {
	new start_address = address;

	while (FindOpcode(address, opcode, end)) {
		if (ReadOperand(address, end) == operand) {
			return 1;
		}
	}

	// Nothing was found
	address = start_address;

	return 0;
}

static stock RedirectArraySlot(...) {
	#emit LOAD.S.pri  12
	#emit LREF.S.alt  16
	#emit SHL.C.alt   2
	#emit ADD
	#emit PUSH.pri

	#emit LREF.S.pri  20
	#emit POP.alt
	#emit SUB
	#emit STOR.I
}

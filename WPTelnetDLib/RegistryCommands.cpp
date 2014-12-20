#include "pch.h"
#include "RegistryCommands.h"


RegCommand::RegCommand() {

}


void ProcessChangeRegPath(RegContext* pContext, Connection *pConnection,string pPath){
	HKEY key = NULL;
	DWORD error = ERROR_SUCCESS;
	string newPath = "";
	if (pPath == "\\") {
		key = pContext->rootKey;
	}else
	if (pPath == ".."){
		std::size_t found = pContext->regPath.find_last_of("\\");
		if (found == std::string::npos || found == 0)
			key = pContext->rootKey;
		else {
			newPath = pContext->regPath.substr(0,found );
			error = RegOpenKeyExA(pContext->rootKey, newPath.c_str(), 0, KEY_WRITE | KEY_READ, &key);
			if (error != ERROR_SUCCESS)
				error = RegOpenKeyExA(pContext->rootKey, newPath.c_str(), 0, KEY_READ, &key);
		}
	}
	else {
		if (pContext->regPath != "")
			newPath = pContext->regPath + "\\" + pPath;
		else
			newPath = pPath;
		error = RegOpenKeyExA(pContext->rootKey, newPath.c_str(), 0, KEY_WRITE | KEY_READ, &key);
		if (error != ERROR_SUCCESS)
			error = RegOpenKeyExA(pContext->rootKey, newPath.c_str(), 0, KEY_READ, &key);
		
		if (error != ERROR_SUCCESS) {
			error = RegOpenKeyExA(pContext->rootKey, pPath.c_str(), 0, KEY_WRITE | KEY_READ, &key);
			if (error != ERROR_SUCCESS)
				error = RegOpenKeyExA(pContext->rootKey, pPath.c_str(), 0, KEY_READ, &key);
			newPath = pPath;
		}
	}


	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to open registry key %d", error);
		return;
	}

	if (pContext->currentKey != pContext->rootKey)
		RegCloseKey(pContext->currentKey);
	pContext->regPath = newPath;
	pContext->currentKey = key;
}

class RegProcessHost : public ICommandProcessorHost
{
private:
	RegContext *_context;
public:
	RegProcessHost(RegContext*pContext) {
		_context = pContext;
	}
	virtual void PrintPrompt(Connection *pConnection) {
		
		if (_context->rootKey == HKEY_LOCAL_MACHINE) pConnection->Write("HKLM\\");
		else if (_context->rootKey == HKEY_CLASSES_ROOT) pConnection->Write("HKCR\\");
		else if (_context->rootKey == HKEY_CURRENT_USER) pConnection->Write("HKCU\\");
		else if (_context->rootKey == HKEY_USERS) pConnection->Write("HKU\\");
		else if (_context->rootKey == HKEY_CURRENT_CONFIG) pConnection->Write("HKCC\\");
		else if (_context->rootKey == HKEY_PERFORMANCE_DATA) pConnection->Write("HKPD\\");

		pConnection->Write("%s>", _context->regPath.c_str());
	}
	virtual void UnhandledLine(Connection *pConnection, string pLine){
		ProcessChangeRegPath(_context, pConnection, pLine);
	}
};

void RegCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	RegContext *context = new RegContext();
	context->rootKey = HKEY_LOCAL_MACHINE;
	context->currentKey = HKEY_LOCAL_MACHINE;
	context->regPath = "";
	vector<BaseCommand *> commands = vector<BaseCommand *>();
	commands.push_back((BaseCommand*)new RegHelpCommand(context));
	commands.push_back((BaseCommand*)new RegListCommand(context));
	commands.push_back((BaseCommand*)new RegOpenCommand(context));
	commands.push_back((BaseCommand*)new RegRootCommand(context));
	commands.push_back((BaseCommand*)new FindRegCommand(context));
	commands.push_back((BaseCommand*)new FindRegWriteableCommand(context));
	commands.push_back((BaseCommand*)new RegStrCommand(context));
	commands.push_back((BaseCommand*)new RegDwordCommand(context));
	commands.push_back((BaseCommand*)new RegDelValCommand(context));
	commands.push_back((BaseCommand*)new RegDeleteKeyCommand(context));
	commands.push_back((BaseCommand*)new RegCreateKeyCommand(context));
	commands.push_back((BaseCommand*)new RegDeleteTreeCommand(context));
	RegProcessHost host(context);
	pConnection->WriteLine("Registry Editor - Type HELP for assistance.");
	CommandProcessor processor = CommandProcessor(commands,&host);
	
	processor.PrintPrompt(pConnection);
	const char* line = pConnection->ReadLine();

	while (line != NULL) {
		if (processor.ProcessData(pConnection, line)) {
			
			break;
		}
		line = pConnection->ReadLine();
	}
}

string RegCommand::GetName() {
	return "reg";
}

void PrintRegValueLine(Connection* pConnection, string pValueName,DWORD pValType, BYTE *pValBuf, DWORD pValSize ) {

	pConnection->Write("  %s", pValueName.c_str());
	int i = 0;
	const char *valPtr = (const char*)pValBuf;
	switch (pValType){
	case REG_BINARY:
		pConnection->Write(" binary ");
		for (int i = 0; i < pValSize; i++)
			pConnection->Write("%02x", pValBuf[i]);
		break;
	case REG_DWORD:
		pConnection->Write(" dword %.8x", *(DWORD*)pValBuf);
		break;
	case REG_EXPAND_SZ:
		pConnection->Write(" expand '%s'", pValBuf);
		break;
	case REG_LINK:
		char Temp[1024];
		wcstombs(Temp, (wchar_t *)pValBuf, sizeof(Temp));
		pConnection->Write(" link '%s'", Temp);
		break;
	case REG_MULTI_SZ:
		pConnection->Write(" multiple strings: ");


		while (strlen(valPtr) > 0) {
			pConnection->Write("  %d: %s", i++, valPtr);
			valPtr += strlen((const char*)valPtr);
		}
		pConnection->Write(" end of strings");
		break;
	case REG_QWORD:
		pConnection->Write(" dword %llx", *(long long*)pValBuf);
		break;
	case REG_SZ:
		pConnection->Write(" str '%s'", pValBuf);
		break;
	}
	pConnection->WriteLine("");
}

BYTE *valBuf = NULL;

RegListCommand::RegListCommand(RegContext *pContext) {
	_context = pContext;
	
}

void RegListCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	int index = 0;
	char Temp[1024];
	DWORD TMP;
	DWORD RC = 0;
	bool firstSubKey = true;

	while (RC != ERROR_NO_MORE_ITEMS) {

		RC = RegEnumKeyExA(_context->currentKey, index, Temp, &TMP, 0, NULL, NULL, NULL);
		TMP = sizeof(Temp);
		if (RC == ERROR_SUCCESS) {
			if (firstSubKey){
				pConnection->WriteLine("Subkeys");
				firstSubKey = false;
			}
			pConnection->WriteLine("  %s", Temp);

		}
		index++;
	}

	bool firstValue = true;
	DWORD valType;
	DWORD valSize;
	DWORD valSizeP = 1024 * 1024 * 3;
	if (valBuf == NULL)
		valBuf = (BYTE*)malloc(valSizeP);
	RC = 0;
	index = 0;
	while (RC != ERROR_NO_MORE_ITEMS) {
		valSize = valSizeP;
		RC = RegEnumValueA(_context->currentKey, index, Temp, &TMP, 0, &valType, valBuf, &valSize);
		TMP = sizeof(Temp);
		if (RC == ERROR_SUCCESS) {
			if (firstValue) {
				pConnection->WriteLine("");
				pConnection->WriteLine("Values");
				firstValue = false;
			}
			PrintRegValueLine(pConnection, Temp, valType, valBuf, valSize);
			
			
		}
		index++;
	}
}

string RegListCommand::GetName() {
	return "list";
}


RegOpenCommand::RegOpenCommand(RegContext *pContext) {
	_context = pContext;
}




void RegOpenCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: open path");
		return;
	}
	
	
	ProcessChangeRegPath(_context, pConnection, pCmdLine->GetArgs().at(1));
}

string RegOpenCommand::GetName() {
	return "open";
}


HKEY ParseKey(const char *pStrKey) {
	if (!strcmpi(pStrKey, "HKLM")) return HKEY_LOCAL_MACHINE;
	if (!strcmpi(pStrKey, "HKCR")) return HKEY_CLASSES_ROOT;
	if (!strcmpi(pStrKey, "HKCU")) return HKEY_CURRENT_USER;
	if (!strcmpi(pStrKey, "HKU")) return HKEY_USERS;
	if (!strcmpi(pStrKey, "HKCC")) return HKEY_CURRENT_CONFIG;
	if (!strcmpi(pStrKey, "HKPD")) return HKEY_PERFORMANCE_DATA;
	return NULL;
}

RegRootCommand::RegRootCommand(RegContext *pContext) {
	_context = pContext;
}


void RegRootCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: root HKLM|HKCR|HKCU|HKU|HKCC|HKPD");
		return;
	}
	HKEY key = ParseKey(pCmdLine->GetArgs().at(1).c_str());

	if (key == NULL) {
		pConnection->WriteLine("Invalid Root");
		return;
	}

	if (_context->currentKey != _context->rootKey) 
		RegCloseKey(_context->currentKey);

	_context->currentKey = key;
	_context->rootKey = key;
	_context->regPath = "";
}

string RegRootCommand::GetName() {
	return "root";
}


FindRegCommand::FindRegCommand(RegContext *pContext) {
	_context = pContext;
}

void FindRegCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: findreg keyword ");
		return;
	}
	HKEY key = _context->currentKey;
	pConnection->WriteLine("Searching... ");
	ProcessLevel(pConnection, _context->regPath=="" ? NULL : _context->regPath.c_str(), pCmdLine->GetArgs().at(1).c_str());
	pConnection->WriteLine("Done");

}

bool strcontains(const char *s1, const char *s2)
{
	char    *s, *t, *u;

	if ((s1 == NULL) || (s2 == NULL))
		return false;

	s = strdup(s1);
	if (s == NULL)
		return false;
	t = strdup(s2);
	if (t == NULL)
	{
		free(s);
		return false;
	}
	strupr(s);
	strupr(t);

	bool result = strstr(s, t) != NULL;
	free(s);
	free(t);
	return result;
}

void FindRegCommand::ProcessLevel(Connection *pConnection,const char *pSubKey, const char *pKeyword) {
	bool isMatch = false;
	HKEY key;

	DWORD error = RegOpenKeyExA(_context->rootKey, pSubKey, 0, KEY_READ, &key);

	if (error != ERROR_SUCCESS){
		return;
	}

	int index = 0;
	char Temp[1024];
	DWORD TMP;
	DWORD RC = 0;

	std::list<string> subKeys;
	string basePath = !pSubKey ? "" : string(pSubKey) + "\\";
	while (RC != ERROR_NO_MORE_ITEMS) {
		RC = RegEnumKeyExA(key, index, Temp, &TMP, 0, NULL, NULL, NULL);
		TMP = sizeof(Temp);
		if (RC == ERROR_SUCCESS && strcontains(Temp, pKeyword)) {
			isMatch = true;
		}

		subKeys.push_back(basePath + Temp);
		index++;
	}


	DWORD valType;
	DWORD valSize;
	DWORD valSizeP = 1024 * 1024 * 3;
	if (valBuf == NULL)
		valBuf = (BYTE*)malloc(valSizeP);
	RC = 0;
	index = 0;

	while (RC != ERROR_NO_MORE_ITEMS) {
		valSize = valSizeP;
		RC = RegEnumValueA(key, index, Temp, &TMP, 0, &valType, valBuf, &valSize);
		TMP = sizeof(Temp);
		if (RC == ERROR_SUCCESS) {
			bool thisIsMatch = false;
			if (strcontains(Temp, pKeyword)) {
				isMatch = true;
				thisIsMatch = true;
			}

			int i = 0;
			const char *valPtr = (const char*)valBuf;
			switch (valType){
			case REG_BINARY:

				break;
			case REG_DWORD:

				break;
			case REG_EXPAND_SZ:
				if (strcontains((const char*)valBuf, pKeyword)){
					isMatch = true;
					thisIsMatch = true;
				}

				break;
			case REG_LINK:
				char Temp2[1024];
				wcstombs(Temp2, (wchar_t *)valBuf, sizeof(Temp2));
				if (strcontains((const char*)Temp2, pKeyword)){
					isMatch = true;
					thisIsMatch = true;
				}

				break;
			case REG_MULTI_SZ:
				while (strlen(valPtr) > 0) {
					if (strcontains((const char*)valPtr, pKeyword)){
						isMatch = true;
						thisIsMatch = true;
					}
					valPtr += strlen((const char*)valPtr)+1;
				}
				break;
			case REG_QWORD:

				break;
			case REG_SZ:
				if (strcontains((const char*)valBuf, pKeyword)){
					isMatch = true;
					thisIsMatch = true;
				}

				break;
			}
			if (thisIsMatch)
				PrintRegValueLine(pConnection, Temp, valType, valBuf, valSize);
		}
		index++;
	}
	RegCloseKey(key);
	
	if (isMatch)
		pConnection->WriteLine("Match: %s", pSubKey ? pSubKey : "");
	subKeys.unique();
	for each (string sub in subKeys)
	{

		ProcessLevel(pConnection, sub.c_str(), pKeyword);
	}
}

string FindRegCommand::GetName() {
	return "find";
}


FindRegWriteableCommand::FindRegWriteableCommand(RegContext *pContext) {
	_context = pContext;
}



void FindRegWriteableCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 1)
	{
		pConnection->WriteLine("SYNTAX: writeable");
		return;
	}

	HKEY key = _context->currentKey;
	pConnection->WriteLine("Searching... ");
	ProcessLevel(pConnection, _context->regPath == "" ? NULL : _context->regPath.c_str());
	pConnection->WriteLine("Done");

}


void FindRegWriteableCommand::ProcessLevel(Connection *pConnection, const char *pSubKey) {
	bool isMatch = false;
	HKEY key;

	DWORD error = RegOpenKeyExA(_context->rootKey, pSubKey, 0, KEY_READ, &key);

	if (error != ERROR_SUCCESS){
		return;
	}

	int index = 0;
	char Temp[1024];
	DWORD TMP;
	DWORD RC = 0;
	std::list<string> subKeys;
	string basePath = !pSubKey ? "" : string(pSubKey) + "\\";
	while (RC != ERROR_NO_MORE_ITEMS) {
		RC = RegEnumKeyExA(key, index, Temp, &TMP, 0, NULL, NULL, NULL);
		TMP = sizeof(Temp);
		

		subKeys.push_back(basePath + Temp);
		index++;
	}
	RegCloseKey(key);
	error = RegOpenKeyExA(_context->rootKey, pSubKey, 0, KEY_WRITE, &key);
	if (error == ERROR_SUCCESS){
		pConnection->WriteLine("Writable!: %s", pSubKey ? pSubKey : "");
		RegCloseKey(key);
	}
	else if(RegOpenKeyExA(_context->rootKey, pSubKey, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS){
		pConnection->WriteLine("Writable (value only)!: %s", pSubKey ? pSubKey : "");
		RegCloseKey(key);
	}
	else if (RegOpenKeyExA(_context->rootKey, pSubKey, 0, KEY_CREATE_SUB_KEY, &key) == ERROR_SUCCESS){
		pConnection->WriteLine("Writable (subkey only)!: %s", pSubKey ? pSubKey : "");
		RegCloseKey(key);
	}
	
	subKeys.unique();
	for each (string sub in subKeys)
	{

		ProcessLevel(pConnection, sub.c_str());
	}
}

string FindRegWriteableCommand::GetName() {
	return "writeable";
}


RegDwordCommand::RegDwordCommand(RegContext *pContext) {
	_context = pContext;
}

void RegDwordCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 3)
	{
		pConnection->WriteLine("SYNTAX: dword name numvalue");
		return;
	}
	
	int index = 0;
	char Temp[1024];
	DWORD value = atoi(pCmdLine->GetArgs().at(2).c_str());
	DWORD valSize = sizeof(DWORD);
	DWORD error = RegSetValueExA(_context->currentKey, pCmdLine->GetArgs().at(1).c_str(), 0, REG_DWORD, (BYTE*)&value, valSize);
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to write to registry %d", error);
		return;
	}
}

string RegDwordCommand::GetName() {
	return "dword";
}


RegStrCommand::RegStrCommand(RegContext *pContext) {
	_context = pContext;
}

void RegStrCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 3)
	{
		pConnection->WriteLine("SYNTAX: string name numvalue");
		return;
	}

	DWORD error;

	int index = 0;
	char Temp[1024];
	char *val = strdup(pCmdLine->GetArgs().at(2).c_str());
	error = RegSetValueExA(_context->currentKey, pCmdLine->GetArgs().at(1).c_str(), 0, REG_SZ, (const BYTE*)val, strlen(val) + 1);
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to write to registry %d", error);
		return;
	}
}

string RegStrCommand::GetName() {
	return "string";
}

RegDelValCommand::RegDelValCommand(RegContext *pContext) {
	_context = pContext;
}

void RegDelValCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: delval name");
		return;
	}


	DWORD error = RegDeleteValueA(_context->currentKey, pCmdLine->GetArgs().at(1).c_str());
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to delete value: %d", error);
		return;
	}
}

string RegDelValCommand::GetName() {
	return "delval";
}


RegCreateKeyCommand::RegCreateKeyCommand(RegContext *pContext) {
	_context = pContext;
}

void RegCreateKeyCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: mkkey <subkey>");
		return;
	}

	HKEY resultKey;
	DWORD error = RegCreateKeyExA(_context->currentKey, pCmdLine->GetArgs().at(1).c_str(),0,NULL, 0, NULL,NULL,&resultKey, NULL);
	RegCloseKey(resultKey);
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to create key: %d", error);
		return;
	}
}

string RegCreateKeyCommand::GetName() {
	return "mkkey";
}


RegDeleteKeyCommand::RegDeleteKeyCommand(RegContext *pContext) {
	_context = pContext;
}

void RegDeleteKeyCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: delkey <subkey>");
		return;
	}
	string keyName = pCmdLine->GetArgs().at(1);
	wstring wideName = wstring(keyName.begin(), keyName.end());
	HKEY resultKey;
	DWORD error = RegDeleteKeyExW(_context->currentKey, wideName.c_str(), 0, NULL);
	RegCloseKey(resultKey);
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to delete key: %d", error);
		return;
	}
}

string RegDeleteKeyCommand::GetName() {
	return "delkey";
}


RegDeleteTreeCommand::RegDeleteTreeCommand(RegContext *pContext) {
	_context = pContext;
}

void RegDeleteTreeCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {
	if (pCmdLine->GetArgs().size() < 2)
	{
		pConnection->WriteLine("SYNTAX: deltree <subkey>");
		return;
	}
	string keyName = pCmdLine->GetArgs().at(1);
	wstring wideName = wstring(keyName.begin(), keyName.end());
	HKEY resultKey;
	DWORD error = RegDeleteTreeW(_context->currentKey, wideName.c_str());
	RegCloseKey(resultKey);
	if (error != ERROR_SUCCESS){
		pConnection->WriteLine("Failed to delete tree: %d", error);
		return;
	}
}

string RegDeleteTreeCommand::GetName() {
	return "deltree";
}



RegHelpCommand::RegHelpCommand(RegContext *pContext) {
	_context = pContext;
}

void RegHelpCommand::ProcessCommand(Connection *pConnection, ParsedCommandLine *pCmdLine) {

	pConnection->WriteLine("help - Shows this screen");
	pConnection->WriteLine("exit - Exits the registry editor");
	pConnection->WriteLine("list - Lists values and subkeys of current path");
	pConnection->WriteLine("open [path] - Opens a registry key");
	pConnection->WriteLine("root HKLM|HKCU|HKU|HKCR|HKCC|HKPD - Sets the root key");
	pConnection->WriteLine("find [str] - Finds a string in the current key and subkeys");
	pConnection->WriteLine("writeable - Lists all writable entries in current key and subkeys");
	pConnection->WriteLine("dword [valuename] [decimalvalue] - Sets a dword values");
	pConnection->WriteLine("string [valuename] [string] - Sets a string");
	pConnection->WriteLine("delval [valuename] - Deletes a value");
	pConnection->WriteLine("delkey [subkey] - Deletes a subkey");
	pConnection->WriteLine("deltree [subkey] - Deletes a subkey and all its children");
	pConnection->WriteLine("mkkey [subkey] - Creates a new subkey");
	
	pConnection->WriteLine("");
	pConnection->WriteLine("A path can also be opened by just typing it. '..' may be used to goto the parent key and a subkey can be opened by just specifying its name.");
	
}

string RegHelpCommand::GetName() {
	return "help";
}
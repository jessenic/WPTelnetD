#include "pch.h"
#include "CommandProcessor.h"
#include "Networking.h"

#include<vector>
using namespace std;

ParsedCommandLine::ParsedCommandLine(string pCommandLine, IExecutionContext *pHost) {		
	int pos = 0;
	_arguments = vector<string>();
	_host = pHost;
	_originalLine = "";
	while (pos < pCommandLine.length()) {
		_originalLine += pCommandLine.substr(pos, pCommandLine.find_first_of('%', pos)-pos);
		if (pCommandLine.find_first_of('%', pos) == string::npos){			
			break;
		}
		else {
			int start = pCommandLine.find_first_of('%', pos) + 1;
			if (pCommandLine.find_first_of('%', start) == string::npos){
				_originalLine += "%";
				pos = start;
				continue;
			}
			int end = pCommandLine.find_first_of('%', start)-1;
			
			if (end<start)

				_originalLine += "%";
			else{
				string varName = pCommandLine.substr(start, end-start+1);
				_originalLine += pHost->GetVariable(varName);
			}
			
			pos = end + 2;

		}
	}

	pos = 0;

	for (;;) {	
		bool skip = false;
		while(pCommandLine[pos]==' ')
			pos++;
		if(pCommandLine[pos] == '"') {
			if(pCommandLine[pos+1]=='"') 
			{
				pos++;
			} 
			else {
				int nextq =  pCommandLine.find('"', pos+1);
				if (nextq == string::npos) {
					if(pos < pCommandLine.length())
					{
						string expr(pCommandLine.substr(pos+1, pCommandLine.length()));
						_arguments.push_back(expr);
					}
					skip = true;
					break;
				} else {
					string expr(pCommandLine.substr(pos+1, nextq-pos-1));
					_arguments.push_back(expr);
					pos = nextq+1;
				}			
				skip = true;
			}				
		} 

		if (!skip){
			int next = pCommandLine.find(' ', pos);
			if (next == string::npos) {
				if(pos < pCommandLine.length())
				{
					string expr(pCommandLine.substr(pos, pCommandLine.length()));
					_arguments.push_back(expr);
				}
				break;
			} else {
				string expr(pCommandLine.substr(pos, next-pos));
				_arguments.push_back(expr);
				pos = next+1;
			}
		}
	}
	if(_arguments.size()>0) 
		_command = _arguments.front();
}

string ParsedCommandLine::GetName() {
	return _command;
}

string ParsedCommandLine::GetRaw() {
	return _originalLine;
}

ParsedCommandLine ParsedCommandLine::GetParametersAsLine(){
	if (_arguments.size() <= 1)
		return ParsedCommandLine(_originalLine,_host);

	return ParsedCommandLine(_originalLine.substr(_command.length() + 1, string::npos),_host);
}

vector<string> ParsedCommandLine::GetArgs() {
	return _arguments;
}		

IExecutionContext *ParsedCommandLine::GetHost(){
	return _host;
}

BaseCommand::BaseCommand() {
}

CommandProcessor::CommandProcessor(vector<BaseCommand *> *pCommands, Connection *pConnection) {
	_commands = pCommands;
	_connection = pConnection;
}



bool CommandProcessor::ProcessCommandLine(ParsedCommandLine *pLine) {
	string cmd = pLine->GetName();
	bool done = false;
	for (std::vector<BaseCommand *>::iterator it = _commands->begin() ; it != _commands->end(); ++it)
		if((*it)->GetName() == cmd) {
			(*it)->ProcessCommand(_connection,pLine);
			done = true;
			break;
		}				  
	
	
	
	return done;
}


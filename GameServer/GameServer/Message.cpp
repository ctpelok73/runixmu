// Message.cpp: implementation of the CMessage class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Message.h"
#include "MemScript.h"
#include "Util.h"

CMessage gMessage;

static bool MessageFileExists(const char* path)
{
	DWORD attributes = GetFileAttributesA(path);

	return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMessage::CMessage() // OK
{
	memset(this->m_DefaultMessage,0,sizeof(this->m_DefaultMessage));

	this->m_MessageInfo.clear();
}

CMessage::~CMessage() // OK
{

}

bool CMessage::LoadFromTxt(const char* path)
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return false;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		delete lpMemScript;
		return false;
	}

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			MESSAGE_INFO info;

			info.Index = lpMemScript->GetNumber();

			strcpy_s(info.Message,lpMemScript->GetAsString());

			this->m_MessageInfo.insert(std::pair<int,MESSAGE_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	return true;
}

bool CMessage::LoadFromXml(const char* path)
{
	pugi::xml_document file;
	pugi::xml_parse_result result = file.load_file(path);

	if (result.status != pugi::status_ok)
	{
		return false;
	}

	pugi::xml_node root = file.child("MessageList");

	if (root.empty())
	{
		return false;
	}

	for (pugi::xml_node node = root.child("Message"); node; node = node.next_sibling("Message"))
	{
		int index = node.attribute("Index").as_int(-1);

		if (index < 0)
		{
			index = node.attribute("ID").as_int(-1);
		}

		if (index < 0)
		{
			index = node.attribute("Id").as_int(-1);
		}

		if (index < 0)
		{
			index = node.attribute("id").as_int(-1);
		}

		if (index < 0)
		{
			continue;
		}

		const char* text = node.attribute("Text").as_string();

		if (text[0] == 0)
		{
			text = node.child_value();
		}

		MESSAGE_INFO info;
		info.Index = index;
		strcpy_s(info.Message, text);
		this->m_MessageInfo[index] = info;
	}

	return (this->m_MessageInfo.empty() == 0);
}

void CMessage::Load(char* path, const char* language) // OK
{
	this->m_MessageInfo.clear();

	std::vector<std::string> candidateList;
	std::string basePath = path;

	size_t slash = basePath.find_last_of("\\/");

	if (slash != std::string::npos)
	{
		std::string dataRoot = basePath.substr(0, slash + 1);

		if (language != 0 && language[0] != 0)
		{
			candidateList.push_back(dataRoot + "Lang\\" + language + "\\Message.xml");
			candidateList.push_back(dataRoot + "Lang\\" + language + "\\Message.txt");
		}

		candidateList.push_back(dataRoot + "Lang\\Message.xml");
		candidateList.push_back(dataRoot + "Lang\\Message.txt");
		candidateList.push_back(dataRoot + "Message.xml");
	}

	candidateList.push_back(basePath);

	for (std::vector<std::string>::iterator it = candidateList.begin(); it != candidateList.end(); ++it)
	{
		const std::string& candidate = (*it);

		if (MessageFileExists(candidate.c_str()) == false)
		{
			continue;
		}

		size_t ext = candidate.find_last_of('.');

		if (ext != std::string::npos && _stricmp(candidate.substr(ext).c_str(), ".xml") == 0)
		{
			if (this->LoadFromXml(candidate.c_str()) != 0)
			{
				LogAdd(LOG_BLUE, "[Message] Loaded XML: %s", candidate.c_str());
				return;
			}
		}
		else
		{
			if (this->LoadFromTxt(candidate.c_str()) != 0)
			{
				LogAdd(LOG_BLUE, "[Message] Loaded TXT: %s", candidate.c_str());
				return;
			}
		}
	}

	ErrorMessageBox("Could not load any Message file");
}

char* CMessage::GlobalText(int index) // OK
{
	std::map<int,MESSAGE_INFO>::iterator it = this->m_MessageInfo.find(index);

	if(it == this->m_MessageInfo.end())
	{
		wsprintf(this->m_DefaultMessage,"Could not find message %d!",index);
		return this->m_DefaultMessage;
	}
	else
	{
		return it->second.Message;
	}
}

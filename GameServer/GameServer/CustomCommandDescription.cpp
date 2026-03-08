// CustomCommandDescription.cpp: implementation of the CCustomCommandDescription class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CommandManager.h"
#include "CustomCommandDescription.h"
#include "Log.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "Util.h"

CCustomCommandDescription gCustomCommandDescription;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomCommandDescription::CCustomCommandDescription() // OK
{
	this->m_CustomCommandDescriptionInfo.clear();
}

CCustomCommandDescription::~CCustomCommandDescription() // OK
{

}

void CCustomCommandDescription::Load(char* path) // OK
{
	char xmlPath[MAX_PATH] = { 0 };
	const char* sourcePath = path;
	bool loadXml = false;
	const char* ext = strrchr(path, '.');

	if (ext != 0 && _stricmp(ext, ".xml") == 0)
	{
		loadXml = true;
	}
	else if (ext != 0 && _stricmp(ext, ".txt") == 0)
	{
		strcpy_s(xmlPath, path);
		char* xmlExt = strrchr(xmlPath, '.');

		if (xmlExt != 0)
		{
			strcpy_s(xmlExt, 5, ".xml");
			FILE* file = 0;

			if (fopen_s(&file, xmlPath, "r") == 0 && file != 0)
			{
				fclose(file);
				sourcePath = xmlPath;
				loadXml = true;
			}
		}
	}

	if (loadXml != 0)
	{
		pugi::xml_document file;
		pugi::xml_parse_result res = file.load_file(sourcePath);

		if (res.status != pugi::status_ok)
		{
			ErrorMessageBox("Error load fail: %s", sourcePath);
			return;
		}

		this->m_CustomCommandDescriptionInfo.clear();

		pugi::xml_node root = file.child("CustomCommandDescription");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			CUSTOMCOMMANDDESCRIPTION_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			strcpy_s(info.Commmand, leaf.attribute("Command").as_string());
			strcpy_s(info.Description, leaf.attribute("Description").as_string());

			this->m_CustomCommandDescriptionInfo.insert(std::pair<int,CUSTOMCOMMANDDESCRIPTION_INFO>(info.Index,info));
		}

		LogAdd(LOG_BLUE, "[XML] CustomCommandDescription loaded successfully (%d records) [%s]", (int)this->m_CustomCommandDescriptionInfo.size(), sourcePath);

		return;
	}

	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->m_CustomCommandDescriptionInfo.clear();

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

			CUSTOMCOMMANDDESCRIPTION_INFO info;

			info.Index = lpMemScript->GetNumber();

			strcpy_s(info.Commmand,lpMemScript->GetAsString());

			strcpy_s(info.Description,lpMemScript->GetAsString());

			this->m_CustomCommandDescriptionInfo.insert(std::pair<int,CUSTOMCOMMANDDESCRIPTION_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomCommandDescription loaded successfully (%d records) [%s]", (int)this->m_CustomCommandDescriptionInfo.size(), path);

	delete lpMemScript;
}

bool CCustomCommandDescription::GetInfo(int index,CUSTOMCOMMANDDESCRIPTION_INFO* lpInfo) // OK
{
	std::map<int,CUSTOMCOMMANDDESCRIPTION_INFO>::iterator it = this->m_CustomCommandDescriptionInfo.find(index);

	if(it == this->m_CustomCommandDescriptionInfo.end())
	{
		return 0;
	}
	else
	{
		(*lpInfo) = it->second;
		return 1;
	}
}

bool CCustomCommandDescription::GetInfoByName(LPOBJ lpObj, char* message) // OK
{
#if GAMESERVER_CLIENTE_UPDATE >= 7
	char command[32] = {0};

	memset(command,0,sizeof(command));

	gCommandManager.GetString(message,command,sizeof(command),0);

	for(std::map<int,CUSTOMCOMMANDDESCRIPTION_INFO>::iterator it=this->m_CustomCommandDescriptionInfo.begin();it != this->m_CustomCommandDescriptionInfo.end();it++)
	{
		if(_stricmp(it->second.Commmand,command) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,it->second.Description,lpObj->Name);
			return 1;
		}
	}
#endif
	return 0;
}

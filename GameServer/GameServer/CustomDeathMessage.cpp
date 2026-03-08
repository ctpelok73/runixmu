// CustomDeathMessage.cpp: implementation of the CGate class.//
//////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "CommandManager.h"
#include "CustomDeathMessage.h"
#include "Log.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "ServerInfo.h"
#include "Util.h"


CustomDeathMessage gCustomDeathMessage;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CustomDeathMessage::CustomDeathMessage() // OK
{
	this->m_CustomDeathMessage.clear();
}


CustomDeathMessage::~CustomDeathMessage() // OK
{
}


void CustomDeathMessage::Load(char* path) // OK
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

		this->m_CustomDeathMessage.clear();

		pugi::xml_node root = file.child("CustomDeathMessage");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			CUSTOMDEATHMESSAGE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			strcpy_s(info.Text, leaf.attribute("Text").as_string());

			this->m_CustomDeathMessage.insert(std::pair<int, CUSTOMDEATHMESSAGE_INFO>(info.Index, info));
		}

		LogAdd(LOG_BLUE, "[XML] CustomDeathMessage loaded successfully (%d records) [%s]", (int)this->m_CustomDeathMessage.size(), sourcePath);

		return;
	}

	CMemScript* lpMemScript = new CMemScript;

	if (lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR, path);
		return;
	}


	if (lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}


	this->m_CustomDeathMessage.clear();


	try
	{

		while (true)
		{
			if (lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}


			if (strcmp("end", lpMemScript->GetString()) == 0)
			{
				break;
			}


			CUSTOMDEATHMESSAGE_INFO info;


			info.Index = lpMemScript->GetNumber();


			strcpy_s(info.Text, lpMemScript->GetAsString());


			this->m_CustomDeathMessage.insert(std::pair<int, CUSTOMDEATHMESSAGE_INFO>(info.Index, info));
		}
	}
	catch (...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomDeathMessage loaded successfully (%d records) [%s]", (int)this->m_CustomDeathMessage.size(), path);


	delete lpMemScript;
}


void CustomDeathMessage::GetDeathText(LPOBJ lpTarget, LPOBJ lpObj, int index) // OK
{
#if GAMESERVER_CLIENTE_UPDATE >= 4

	if (gServerInfo.m_CustomDeathMessageSwitch == 0)
	{
		return;
	}

	CUSTOMDEATHMESSAGE_INFO CustomDM;

	if (this->GetInfo(index, &CustomDM) == 0)
	{
		return;
	}

	GCChatTargetSend(lpTarget, lpObj->Index, CustomDM.Text);
#endif
}


bool CustomDeathMessage::GetInfo(int index, CUSTOMDEATHMESSAGE_INFO* lpInfo) // OK
{

	std::map<int, CUSTOMDEATHMESSAGE_INFO>::iterator it = this->m_CustomDeathMessage.find(index);

	if (it == this->m_CustomDeathMessage.end())
	{
		return 0;
	}
	else
	{
		(*lpInfo) = it->second;
		return 1;
	}
}

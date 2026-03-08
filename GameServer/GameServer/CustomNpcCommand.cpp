//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CustomNpcCommand.h"
#include "CommandManager.h"
#include "Map.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "NpcTalk.h"
#include "Path.h"
#include "Log.h"
#include "Util.h"

CCustomNpcCommand gCustomNpcCommand;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomNpcCommand::CCustomNpcCommand() // OK
{
	this->m_CustomNpcCommand.clear();
}

CCustomNpcCommand::~CCustomNpcCommand() // OK
{

}

void CCustomNpcCommand::Load(char* path) // OK
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

		this->m_CustomNpcCommand.clear();

		pugi::xml_node root = file.child("CustomNpcCommand");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			NPC_TYPE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.MonsterClass = leaf.attribute("MonsterClass").as_int();
			info.Map = leaf.attribute("Map").as_int();
			info.X = leaf.attribute("X").as_int();
			info.Y = leaf.attribute("Y").as_int();
			info.Talk = leaf.attribute("Talk").as_int();
			strcpy_s(info.Command, leaf.attribute("Command").as_string());

			this->m_CustomNpcCommand.insert(std::pair<int,NPC_TYPE_INFO>(info.Index,info));
		}

		LogAdd(LOG_BLUE, "[XML] CustomNpcCommand loaded successfully (%d records) [%s]", (int)this->m_CustomNpcCommand.size(), sourcePath);

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

	this->m_CustomNpcCommand.clear();

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

			NPC_TYPE_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.Map = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			info.Talk = lpMemScript->GetAsNumber();

			strcpy_s(info.Command,lpMemScript->GetAsString());

			this->m_CustomNpcCommand.insert(std::pair<int,NPC_TYPE_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomNpcCommand loaded successfully (%d records) [%s]", (int)this->m_CustomNpcCommand.size(), path);

	delete lpMemScript;
}

bool CCustomNpcCommand::GetNpcCommand(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	#if (GAMESERVER_CLIENTE_UPDATE >= 12)
	for(std::map<int,NPC_TYPE_INFO>::iterator it=this->m_CustomNpcCommand.begin();it != this->m_CustomNpcCommand.end();it++)
	{
		if(it->second.MonsterClass == lpNpc->Class && it->second.Map == lpNpc->Map && it->second.X == lpNpc->X && it->second.Y == lpNpc->Y)
		{
			if (it->second.Talk == 1)
			{
				CommandSelect(lpObj,it->second.Command,lpNpc->Index);
			}
			else
			{
				CommandSelect(lpObj,it->second.Command,-1);
			}
			return 1;
		}

	}
#endif
	return 0;
}

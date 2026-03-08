// CustomMove.cpp: implementation of the CCustomMove class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CommandManager.h"
#include "CustomMove.h"
#include "GensSystem.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "Util.h"

CCustomMove gCustomMove;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomMove::CCustomMove() // OK
{
	this->m_CustomMoveInfo.clear();
}

CCustomMove::~CCustomMove() // OK
{

}

void CCustomMove::Load(char* path) // OK
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

		this->m_CustomMoveInfo.clear();

		pugi::xml_node root = file.child("CustomMove");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			CUSTOMMOVE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			strcpy_s(info.Name, leaf.attribute("Name").as_string());
			info.Map = leaf.attribute("Map").as_int();
			info.X = leaf.attribute("X").as_int();
			info.Y = leaf.attribute("Y").as_int();
			info.MinLevel = leaf.attribute("MinLevel").as_int();
			info.MaxLevel = leaf.attribute("MaxLevel").as_int();
			info.MinReset = leaf.attribute("MinReset").as_int();
			info.MaxReset = leaf.attribute("MaxReset").as_int();
			info.MinMReset = leaf.attribute("MinMReset").as_int();
			info.MaxMReset = leaf.attribute("MaxMReset").as_int();
			info.AccountLevel = leaf.attribute("AccountLevel").as_int();
			info.PkMove = leaf.attribute("PkMove").as_int();

			this->m_CustomMoveInfo.insert(std::pair<int,CUSTOMMOVE_INFO>(info.Index,info));
		}

		LogAdd(LOG_BLUE, "[XML] CustomMove loaded successfully (%d records) [%s]", (int)this->m_CustomMoveInfo.size(), sourcePath);

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

	this->m_CustomMoveInfo.clear();

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

			CUSTOMMOVE_INFO info;

			info.Index = lpMemScript->GetNumber();

			strcpy_s(info.Name,lpMemScript->GetAsString());

			info.Map = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			info.MinLevel = lpMemScript->GetAsNumber();

			info.MaxLevel = lpMemScript->GetAsNumber();

			info.MinReset = lpMemScript->GetAsNumber();

			info.MaxReset = lpMemScript->GetAsNumber();

			info.MinMReset = lpMemScript->GetAsNumber();

			info.MaxMReset = lpMemScript->GetAsNumber();

			info.AccountLevel = lpMemScript->GetAsNumber();

			info.PkMove = lpMemScript->GetAsNumber();

			this->m_CustomMoveInfo.insert(std::pair<int,CUSTOMMOVE_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomMove loaded successfully (%d records) [%s]", (int)this->m_CustomMoveInfo.size(), path);

	delete lpMemScript;
}

bool CCustomMove::GetInfo(int index,CUSTOMMOVE_INFO* lpInfo) // OK
{
	std::map<int,CUSTOMMOVE_INFO>::iterator it = this->m_CustomMoveInfo.find(index);

	if(it == this->m_CustomMoveInfo.end())
	{
		return 0;
	}
	else
	{
		(*lpInfo) = it->second;
		return 1;
	}
}

bool CCustomMove::GetInfoByName(LPOBJ lpObj, char* message, int Npc) // OK
{
	char command[32] = {0};

	memset(command,0,sizeof(command));

	gCommandManager.GetString(message,command,sizeof(command),0);


	for(std::map<int,CUSTOMMOVE_INFO>::iterator it=this->m_CustomMoveInfo.begin();it != this->m_CustomMoveInfo.end();it++)
	{
		if(_stricmp(it->second.Name,command) == 0)
		{
			if(it->second.MinLevel != -1 && lpObj->GetLevel() < it->second.MinLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(224),it->second.MinLevel);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(224),it->second.MinLevel);
				}
				return 1;
			}

			if(it->second.MaxLevel != -1 && lpObj->GetLevel() > it->second.MaxLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(230),it->second.MaxLevel);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(230),it->second.MaxLevel);
				}
				return 1;
			}

			if(it->second.MinReset != -1 && lpObj->Reset < it->second.MinReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(231),it->second.MinReset);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(231),it->second.MinReset);
				}
				return 1;
			}

			if(it->second.MaxReset != -1 && lpObj->Reset > it->second.MaxReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(232),it->second.MaxReset);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(232),it->second.MaxReset);
				}
				return 1;
			}

			if(it->second.MinMReset != -1 && lpObj->MasterReset < it->second.MinMReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(818),it->second.MinMReset);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(818),it->second.MinMReset);
				}
				return 1;
			}

			if(it->second.MaxMReset != -1 && lpObj->MasterReset > it->second.MaxMReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(819),it->second.MaxMReset);
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(819),it->second.MaxMReset);
				}
				return 1;
			}

			if(lpObj->AccountLevel < it->second.AccountLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(226));
				}
				return 1;
			}

			if(it->second.PkMove == 0 && lpObj->PKLevel >= 5)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(227));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(227));
				}
				return 1;
			}

			if(lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(226));
				}
				return 1;
			}

			if(it->second.Map == MAP_ATLANS && (lpObj->Inventory[8].IsItem() != 0 && (lpObj->Inventory[8].m_Index == GET_ITEM(13,2) || lpObj->Inventory[8].m_Index == GET_ITEM(13,3)))) // Uniria,Dinorant
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(274));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(274));
				}
				return 1;
			}

			if((it->second.Map == MAP_ICARUS || it->second.Map == MAP_KANTURU3) && (lpObj->Inventory[7].IsItem() == 0 && lpObj->Inventory[8].m_Index != GET_ITEM(13,3) && lpObj->Inventory[8].m_Index != GET_ITEM(13,37))) // Dinorant,Fenrir
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(228));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(228));
				}
				return 1;
			}

			#if(GAMESERVER_UPDATE>=501)

			if(lpObj->GensFamily == GENS_FAMILY_NONE && gMapManager.GetMapGensBattle(it->second.Map) != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(229));
				if (Npc >= 0)
				{
					GCChatTargetNewSend(lpObj,Npc,gMessage.GlobalText(229));
				}
				return 1;
			}

			#endif

			gObjTeleport(lpObj->Index,it->second.Map,it->second.X,it->second.Y);
			gLog.Output(LOG_COMMAND,"[CustomMove][%s][%s] - (MoveIndex: %d)",lpObj->Account,lpObj->Name,it->second.Index);
			return 1;
		}
	}

	return 0;
}

void CCustomMove::GetMove(LPOBJ lpObj,int index) // OK
{
	CUSTOMMOVE_INFO CustomMoveInfo;

	if(this->GetInfo(index,&CustomMoveInfo) == 0)
	{
		return;
	}

	if(CustomMoveInfo.MinLevel != -1 && lpObj->GetLevel() < CustomMoveInfo.MinLevel)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(224),CustomMoveInfo.MinLevel);
		return;
	}

	if(CustomMoveInfo.MaxLevel != -1 && lpObj->GetLevel() > CustomMoveInfo.MaxLevel)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(230),CustomMoveInfo.MaxLevel);
		return;
	}

	if(CustomMoveInfo.MinReset != -1 && lpObj->Reset < CustomMoveInfo.MinReset)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(231),CustomMoveInfo.MinReset);
		return;
	}

	if(CustomMoveInfo.MaxReset != -1 && lpObj->Reset > CustomMoveInfo.MaxReset)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(232),CustomMoveInfo.MaxReset);
		return;
	}

	if(CustomMoveInfo.MinReset != -1 && lpObj->MasterReset < CustomMoveInfo.MinMReset)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(818),CustomMoveInfo.MinMReset);
		return;
	}

	if(CustomMoveInfo.MaxReset != -1 && lpObj->MasterReset > CustomMoveInfo.MaxMReset)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(819),CustomMoveInfo.MaxMReset);
		return;
	}

	if(lpObj->AccountLevel < CustomMoveInfo.AccountLevel)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
		return;
	}

	if(CustomMoveInfo.PkMove == 0 && lpObj->PKLevel >= 5)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(227));
		return;
	}

	if(lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
		return;
	}

	if(CustomMoveInfo.Map == MAP_ATLANS && (lpObj->Inventory[8].IsItem() != 0 && (lpObj->Inventory[8].m_Index == GET_ITEM(13,2) || lpObj->Inventory[8].m_Index == GET_ITEM(13,3)))) // Uniria,Dinorant
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(274));
		return;
	}

	if((CustomMoveInfo.Map == MAP_ICARUS || CustomMoveInfo.Map == MAP_KANTURU3) && (lpObj->Inventory[7].IsItem() == 0 && lpObj->Inventory[8].m_Index != GET_ITEM(13,3) && lpObj->Inventory[8].m_Index != GET_ITEM(13,37))) // Dinorant,Fenrir
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(228));
		return;
	}

	#if(GAMESERVER_UPDATE>=501)

	if(lpObj->GensFamily == GENS_FAMILY_NONE && gMapManager.GetMapGensBattle(CustomMoveInfo.Map) != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(229));
		return;
	}

	#endif

	gObjTeleport(lpObj->Index,CustomMoveInfo.Map,CustomMoveInfo.X,CustomMoveInfo.Y);
	gLog.Output(LOG_COMMAND,"[CustomMove][%s][%s] - (MoveIndex: %d)",lpObj->Account,lpObj->Name,index);
}

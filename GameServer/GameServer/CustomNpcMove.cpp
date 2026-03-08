//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CustomNpcMove.h"
#include "CastleSiegeSync.h"
#include "GensSystem.h"
#include "Map.h"
#include "MapManager.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "NpcTalk.h"
#include "Path.h"
#include "Log.h"
#include "Util.h"

CCustomNpcMove gCustomNpcMove;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomNpcMove::CCustomNpcMove() // OK
{
	this->m_CustomNpcMove.clear();
}

CCustomNpcMove::~CCustomNpcMove() // OK
{

}

void CCustomNpcMove::Load(char* path) // OK
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

		this->m_CustomNpcMove.clear();

		pugi::xml_node root = file.child("CustomNpcMove");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			NPC_MOVE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.MonsterClass = leaf.attribute("MonsterClass").as_int();
			info.Map = leaf.attribute("Map").as_int();
			info.X = leaf.attribute("X").as_int();
			info.Y = leaf.attribute("Y").as_int();
			info.MoveMap = leaf.attribute("MoveMap").as_int();
			info.MoveX = leaf.attribute("MoveX").as_int();
			info.MoveY = leaf.attribute("MoveY").as_int();
			info.MinLevel = leaf.attribute("MinLevel").as_int();
			info.MaxLevel = leaf.attribute("MaxLevel").as_int();
			info.MinReset = leaf.attribute("MinReset").as_int();
			info.MaxReset = leaf.attribute("MaxReset").as_int();
			info.MinMReset = leaf.attribute("MinMReset").as_int();
			info.MaxMReset = leaf.attribute("MaxMReset").as_int();
			info.AccountLevel = leaf.attribute("AccountLevel").as_int();
			info.PkMove = leaf.attribute("PkMove").as_int();

			this->m_CustomNpcMove.insert(std::pair<int,NPC_MOVE_INFO>(info.Index,info));
		}

		LogAdd(LOG_BLUE, "[XML] CustomNpcMove loaded successfully (%d records) [%s]", (int)this->m_CustomNpcMove.size(), sourcePath);

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

	this->m_CustomNpcMove.clear();

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

			NPC_MOVE_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.Map = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			info.MoveMap = lpMemScript->GetAsNumber();

			info.MoveX = lpMemScript->GetAsNumber();

			info.MoveY = lpMemScript->GetAsNumber();

			info.MinLevel = lpMemScript->GetAsNumber();

			info.MaxLevel = lpMemScript->GetAsNumber();

			info.MinReset = lpMemScript->GetAsNumber();

			info.MaxReset = lpMemScript->GetAsNumber();

			info.MinMReset = lpMemScript->GetAsNumber();

			info.MaxMReset = lpMemScript->GetAsNumber();

			info.AccountLevel = lpMemScript->GetAsNumber();

			info.PkMove = lpMemScript->GetAsNumber();

			this->m_CustomNpcMove.insert(std::pair<int,NPC_MOVE_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	LogAdd(LOG_BLUE, "[TXT] CustomNpcMove loaded successfully (%d records) [%s]", (int)this->m_CustomNpcMove.size(), path);

	delete lpMemScript;
}

bool CCustomNpcMove::GetNpcMove(LPOBJ lpObj,int MonsterClass,int Map,int X,int Y) // OK
{
	#if (GAMESERVER_CLIENTE_UPDATE >= 12)
	for(std::map<int,NPC_MOVE_INFO>::iterator it=this->m_CustomNpcMove.begin();it != this->m_CustomNpcMove.end();it++)
	{
		if(it->second.MonsterClass == MonsterClass && it->second.Map == Map && it->second.X == X && it->second.Y == Y)
		{

			if(it->second.MinLevel != -1 && lpObj->GetLevel() < it->second.MinLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(224),it->second.MinLevel);
				return 1;
			}

			if(it->second.MaxLevel != -1 && lpObj->GetLevel() > it->second.MaxLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(230),it->second.MaxLevel);
				return 1;
			}

			if(it->second.MinReset != -1 && lpObj->Reset < it->second.MinReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(231),it->second.MinReset);
				return 1;
			}

			if(it->second.MaxReset != -1 && lpObj->Reset > it->second.MaxReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(232),it->second.MaxReset);
				return 1;
			}

			if(it->second.MinReset != -1 && lpObj->MasterReset < it->second.MinMReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(818),it->second.MinMReset);
				return 1;
			}

			if(it->second.MaxReset != -1 && lpObj->MasterReset > it->second.MaxMReset)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(819),it->second.MaxMReset);
				return 1;
			}

			if(lpObj->AccountLevel < it->second.AccountLevel)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
				return 1;
			}

			if(it->second.PkMove == 0 && lpObj->PKLevel >= 5)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(227));
				return 1;
			}

			if(lpObj->Interface.use != 0 || lpObj->Teleport != 0 || lpObj->DieRegen != 0 || lpObj->PShopOpen != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(226));
				return 1;
			}

			if(it->second.MoveMap == MAP_ATLANS && (lpObj->Inventory[8].IsItem() != 0 && (lpObj->Inventory[8].m_Index == GET_ITEM(13,2) || lpObj->Inventory[8].m_Index == GET_ITEM(13,3)))) // Uniria,Dinorant
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(274));
				return 1;
			}

			if((it->second.MoveMap == MAP_ICARUS || it->second.MoveMap == MAP_KANTURU3) && (lpObj->Inventory[7].IsItem() == 0 && lpObj->Inventory[8].m_Index != GET_ITEM(13,3) && lpObj->Inventory[8].m_Index != GET_ITEM(13,37))) // Dinorant,Fenrir
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(228));
				return 1;
			}

			#if(GAMESERVER_UPDATE>=501)

			if(lpObj->GensFamily == GENS_FAMILY_NONE && gMapManager.GetMapGensBattle(it->second.MoveMap) != 0)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GlobalText(229));
				return 1;
			}

			#endif
			gObjTeleport(lpObj->Index,it->second.MoveMap,it->second.MoveX,it->second.MoveY);
			return 1;
		}

	}
#endif
	return 0;
}

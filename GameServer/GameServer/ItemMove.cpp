// ItemMove.cpp: implementation of the CItemMove class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ItemMove.h"
#include "ItemManager.h"
#include "MemScript.h"
#include "Util.h"

CItemMove gItemMove;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CItemMove::CItemMove() // OK
{
	this->m_ItemMoveInfo.clear();
}

CItemMove::~CItemMove() // OK
{

}

void CItemMove::Load(char* path) // OK
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

		this->m_ItemMoveInfo.clear();

		pugi::xml_node root = file.child("ItemMove");

		for (pugi::xml_node leaf = root.child("Info"); leaf; leaf = leaf.next_sibling("Info"))
		{
			ITEM_MOVE_INFO info;

			info.Index = leaf.attribute("Index").as_int();
			info.AllowDrop = leaf.attribute("AllowDrop").as_int(leaf.attribute("BanDrop").as_int(1));
			info.AllowSell = leaf.attribute("AllowSell").as_int(leaf.attribute("BanSell").as_int(1));
			info.AllowTrade = leaf.attribute("AllowTrade").as_int(leaf.attribute("BanTrade").as_int(1));
			info.AllowVault = leaf.attribute("AllowVault").as_int(leaf.attribute("BanVaul").as_int(leaf.attribute("BanVault").as_int(1)));

			this->m_ItemMoveInfo.insert(type_move_item::value_type(info.Index,info));
		}

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

	this->m_ItemMoveInfo.clear();

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

			ITEM_MOVE_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.AllowDrop = lpMemScript->GetAsNumber();

			info.AllowSell = lpMemScript->GetAsNumber();

			info.AllowTrade = lpMemScript->GetAsNumber();

			info.AllowVault = lpMemScript->GetAsNumber();

			this->m_ItemMoveInfo.insert(type_move_item::value_type(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

void CItemMove::ExportXML(std::string filename)
{
	pugi::xml_node leaf;

	pugi::xml_document doc;

	pugi::xml_node root = doc.append_child("ItemMove");

	for (type_move_item::iterator it = m_ItemMoveInfo.begin(); it != m_ItemMoveInfo.end(); it++)
	{
		ITEM_MOVE_INFO* info = &it->second;

		if (info)
		{
			leaf = root.append_child("Info");

			leaf.append_attribute("Index").set_value(info->Index);

			leaf.append_attribute("BanDrop").set_value(info->AllowDrop);

			leaf.append_attribute("BanSell").set_value(info->AllowSell);

			leaf.append_attribute("BanTrade").set_value(info->AllowTrade);

			leaf.append_attribute("BanVaul").set_value(info->AllowVault);

			if (info->Index != -1)
			{
				leaf.append_attribute("Comment").set_value(gItemManager.GetItemName(info->Index));
			}
			else
			{
				leaf.append_attribute("Comment").set_value(" ");
			}
		}
	}

	doc.save_file(filename.c_str());
}

bool CItemMove::CheckItemMoveAllowDrop(int index) // OK
{
	std::map<int,ITEM_MOVE_INFO>::iterator it = this->m_ItemMoveInfo.find(index);

	if(it == this->m_ItemMoveInfo.end())
	{
		return 1;
	}
	else
	{
		return ((it->second.AllowDrop==0)?0:1);
	}
}

bool CItemMove::CheckItemMoveAllowSell(int index) // OK
{
	std::map<int,ITEM_MOVE_INFO>::iterator it = this->m_ItemMoveInfo.find(index);

	if(it == this->m_ItemMoveInfo.end())
	{
		return 1;
	}
	else
	{
		return ((it->second.AllowSell==0)?0:1);
	}
}

bool CItemMove::CheckItemMoveAllowTrade(int index) // OK
{
	std::map<int,ITEM_MOVE_INFO>::iterator it = this->m_ItemMoveInfo.find(index);

	if(it == this->m_ItemMoveInfo.end())
	{
		return 1;
	}
	else
	{
		return ((it->second.AllowTrade==0)?0:1);
	}
}

bool CItemMove::CheckItemMoveAllowVault(int index) // OK
{
	std::map<int,ITEM_MOVE_INFO>::iterator it = this->m_ItemMoveInfo.find(index);

	if(it == this->m_ItemMoveInfo.end())
	{
		return 1;
	}
	else
	{
		return ((it->second.AllowVault==0)?0:1);
	}
}

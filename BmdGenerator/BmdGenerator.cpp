// Standalone BMD generator, независимый от GameServer/ServerInfo
// Читает Item\Item.xml и пишет BMD\Local\Eng\Item.bmd в формате Script_Item

#include <windows.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "..\\GameServer\\GameServer\\pugixml.hpp"

#define MAX_CLASS 15
#define MAX_RESISTANCE_TYPE 7
#define MAX_ITEM_SECTION 16
#define MAX_ITEM_INDEX 512

namespace ItemKind1Local
{
	enum
	{
		SKILL = 10,
		GUARDIAN_MOUNT = 20,
	};
}

namespace ItemKind2Local
{
	enum
	{
		STAFF = 12,
		STICK = 13,
		BOOK = 14,
		RUNIC_MACE = 78,
		MAGIC_GUN = 84,
		MAGIC_STICK = 89,
		MAGIC_ORB = 90,
	};
}

namespace SetItemOptionLocal
{
	enum
	{
		ADD_STRENGTH = 0,
		ADD_DEXTERITY = 1,
		ADD_ENERGY = 2,
		ADD_VITALITY = 3,
		ADD_LEADERSHIP = 4,
	};
}

struct SCRIPT_ITEM_BMD
{
	int   Type;
	short Index;
	short SubIndex;
	char  Name[64];
	BYTE  Kind1;
	BYTE  Kind2;
	BYTE  Kind3;
	bool  TwoHand;
	WORD  Level;
	BYTE  m_byItemSlot;
	WORD  m_wSkillIndex;
	BYTE  Width;
	BYTE  Height;
	WORD  DamageMin;
	WORD  DamageMax;
	BYTE  SuccessfulBlocking;
	WORD  Defense;
	WORD  MagicDefense;
	BYTE  WeaponSpeed;
	BYTE  WalkSpeed;
	BYTE  Durability;
	BYTE  MagicDurability;
	DWORD MagicPower;
	WORD  RequireStrength;
	WORD  RequireDexterity;
	WORD  RequireEnergy;
	WORD  RequireVitality;
	WORD  RequireCharisma;
	WORD  RequireLevel;
	BYTE  Value;
	int   iZen;
	BYTE  AttType;
	BYTE  RequireClass[MAX_CLASS];
	BYTE  Resistance[MAX_RESISTANCE_TYPE];
	bool  Dropinventory;
	bool  Trade;
	bool  StorePersonal;
	bool  WhareHouse;
	bool  SellNpc;
	bool  Expensive;
	bool  Repair;
	WORD  Overlap;
	WORD  PcFlag;
	WORD  MuunFlag;
	DWORD PowerATTK;
};

static BYTE g_BuxCode[3] = { 0xFC,0xCF,0xAB };

static void BuxConvertLocal(BYTE* buf,int size)
{
	for(int i=0;i<size;i++)
	{
		buf[i] ^= g_BuxCode[i%3];
	}
}

static DWORD GenerateCheckSum2Local(BYTE* buf,int size,WORD key)
{
	DWORD dwKey = key;
	DWORD dwResult = (DWORD)(key << 9);

	for(int checked=0;checked<=size-4;checked+=4)
	{
		DWORD temp;
		memcpy(&temp,buf+checked,4);

		DWORD sel = (key + (checked>>2)) % 2;
		if(sel == 0)
			dwResult ^= temp;
		else
			dwResult += temp;

		if(!(checked % 0x10))
		{
			dwResult ^= (unsigned int)(dwResult + dwKey) >> ((checked>>2)%8 + 1);
		}
	}

	return dwResult;
}

static bool PackFileEncryptLocal(const char* filename,BYTE* data,int count,int elemSize,DWORD key,bool writeMax,bool withCheck)
{
	FILE* fp = fopen(filename,"wb");
	if(!fp)
		return false;

	DWORD maxBuffer = (DWORD)count * elemSize;

	std::vector<BYTE> buffer(maxBuffer);

	if(writeMax)
	{
		fwrite(&count,4,1,fp);
	}

	for(int i=0;i<count;i++)
	{
		BuxConvertLocal(data,elemSize);
		memcpy(&buffer[i*elemSize],data,elemSize);
		fwrite(&buffer[i*elemSize],elemSize,1,fp);
		data += elemSize;
	}

	if(withCheck)
	{
		DWORD checksum = GenerateCheckSum2Local(buffer.data(),maxBuffer,(WORD)key);
		fwrite(&checksum,4,1,fp);
	}

	fclose(fp);
	return true;
}

static bool LoadSetItemType(const char* path,std::unordered_map<int,int>& statTypeByIndex)
{
	FILE* fp = fopen(path,"rb");
	if(!fp)
	{
		return false;
	}

	fseek(fp,0,SEEK_END);
	long size = ftell(fp);
	fseek(fp,0,SEEK_SET);

	if(size <= 0)
	{
		fclose(fp);
		return false;
	}

	std::string data;
	data.resize((size_t)size);
	fread(&data[0],1,(size_t)size,fp);
	fclose(fp);

	size_t i = 0;
	if(data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
	{
		i = 3;
	}

	std::vector<std::string> tokens;
	std::string token;
	bool inComment = false;

	for(; i < data.size(); i++)
	{
		char ch = data[i];
		char next = (i + 1 < data.size() ? data[i + 1] : '\0');

		if(inComment)
		{
			if(ch == '\n')
			{
				inComment = false;
			}
			continue;
		}

		if(ch == '/' && next == '/')
		{
			inComment = true;
			i++;
			continue;
		}

		if(ch == '#')
		{
			inComment = true;
			continue;
		}

		if(ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ')
		{
			if(!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}
			continue;
		}

		token.push_back(ch);
	}

	if(!token.empty())
	{
		tokens.push_back(token);
	}

	for(size_t t = 0; t < tokens.size();)
	{
		if(tokens[t] == "end")
		{
			break;
		}

		if(t + 4 >= tokens.size())
		{
			break;
		}

		int section = (int)strtol(tokens[t].c_str(),0,10);
		int index = (int)strtol(tokens[t + 1].c_str(),0,10);
		int statType = (int)strtol(tokens[t + 2].c_str(),0,10);

		int fullIndex = section * MAX_ITEM_INDEX + index;
		statTypeByIndex[fullIndex] = statType;

		t += 5;
	}

	return !statTypeByIndex.empty();
}

static int GetSetItemStatType(const std::unordered_map<int,int>& statTypeByIndex,int itemIndex)
{
	auto it = statTypeByIndex.find(itemIndex);
	if(it == statTypeByIndex.end())
	{
		return -1;
	}
	return it->second;
}

static bool GenerateItemBmd()
{
	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file("Item\\Item.xml");
	if(res.status != pugi::status_ok)
	{
		MessageBoxW(0,L"Не удалось открыть Item\\Item.xml",L"BMD Generator",MB_OK|MB_ICONERROR);
		return false;
	}

	std::vector<SCRIPT_ITEM_BMD> items;

	pugi::xml_node root = doc.child("ItemList");
	if(!root)
	{
		MessageBoxW(0,L"В Item.xml нет корня <ItemList>",L"BMD Generator",MB_OK|MB_ICONERROR);
		return false;
	}

	static const char* ClassNamesLong[] = {"DarkWizard", "DarkKnight", "FairyElf", "MagicGladiator", "DarkLord", "Summoner", "RageFighter", "GrowLancer", "RuneWizard", "Slayer", "GunCrusher", "Kundun", "LemuriaMage", "IllusionKnight", "Alquimista"};
	static const char* ClassNamesShort[] = {"DW", "DK", "FE", "MG", "DL", "SM", "RG", "GL", "RW", "SL", "GC", "KN", "LM", "IK", "AQ"};

	std::vector<bool> classFilter(MAX_CLASS,true);
	for(int c=0;c<MAX_CLASS && c< (int)(sizeof(ClassNamesLong)/sizeof(ClassNamesLong[0]));c++)
	{
		wchar_t prompt[256];
		wsprintfW(prompt,L"Включить класс: %S (%S)?\nДа — включить, Нет — пропустить,\nОтмена — оставить все классы.",ClassNamesLong[c],ClassNamesShort[c]);
		int ans = MessageBoxW(0,prompt,L"BMD Generator",MB_YESNOCANCEL|MB_ICONQUESTION);
		if(ans == IDCANCEL)
		{
			for(int k=0;k<MAX_CLASS;k++) classFilter[k]=true;
			break;
		}
		classFilter[c] = (ans == IDYES);
	}

	std::unordered_map<int,int> setItemStatType;
	LoadSetItemType("Item\\SetItemType.txt",setItemStatType);

	for(pugi::xml_node section = root.child("Section"); section; section = section.next_sibling("Section"))
	{
		int secIndex = section.attribute("Index").as_int();

		for(pugi::xml_node node = section.child("Item"); node; node = node.next_sibling("Item"))
		{
			SCRIPT_ITEM_BMD si;
			memset(&si,0,sizeof(si));

			int subIndex = node.attribute("Index").as_int();
			int fullIndex = secIndex * MAX_ITEM_INDEX + subIndex;

			si.Type     = fullIndex;
			si.Index    = (short)secIndex;
			si.SubIndex = (short)subIndex;

			const char* name = node.attribute("Name").as_string();
			strncpy_s(si.Name,name,_TRUNCATE);

			si.Kind1 = (BYTE)node.attribute("Kind1").as_int();
			si.Kind2 = (BYTE)node.attribute("Kind2").as_int();
			si.Kind3 = (BYTE)node.attribute("Kind3").as_int();

			si.m_byItemSlot  = (BYTE)node.attribute("Slot").as_int();
			si.Width         = (BYTE)node.attribute("Width").as_int();
			si.Height        = (BYTE)node.attribute("Height").as_int();

			si.Value = (BYTE)node.attribute("Value").as_int();
			si.iZen  =          node.attribute("iZen").as_int();

			si.m_wSkillIndex = (WORD)node.attribute("SkillIndex").as_int();

			int dropLevel = node.attribute("DropLevel").as_int();
			si.Level = (WORD)dropLevel;

			if(secIndex != 14)
			{
				si.TwoHand        = node.attribute("TwoHand").as_int() != 0;
				si.DamageMin      = (WORD)node.attribute("DamageMin").as_int();
				si.DamageMax      = (WORD)node.attribute("DamageMax").as_int();
				si.Defense        = (WORD)node.attribute("Defense").as_int();
				si.MagicDefense   = (WORD)node.attribute("MagicDefense").as_int();
				si.SuccessfulBlocking = (BYTE)node.attribute("SuccessfulBlocking").as_int();
				si.WeaponSpeed    = (BYTE)node.attribute("WeaponSpeed").as_int();
				si.WalkSpeed      = (BYTE)node.attribute("WalkSpeed").as_int();

				bool magicDurability = (si.Kind2 == ItemKind2Local::STAFF || si.Kind2 == ItemKind2Local::STICK || si.Kind2 == ItemKind2Local::BOOK || si.Kind2 == ItemKind2Local::RUNIC_MACE || si.Kind2 == ItemKind2Local::MAGIC_GUN || si.Kind2 == ItemKind2Local::MAGIC_STICK || si.Kind2 == ItemKind2Local::MAGIC_ORB);
				if(magicDurability)
				{
					si.Durability = (BYTE)node.attribute("MagicDurability").as_int();
					si.MagicDurability = si.Durability;
				}
				else
				{
					si.Durability = (BYTE)node.attribute("Durability").as_int();
					si.MagicDurability = 0;
				}

				si.MagicPower     = (DWORD)node.attribute("Magicpower").as_int();

				si.RequireLevel   = (WORD)node.attribute("ReqLevel").as_int();

				if(secIndex != 13)
				{
					si.RequireStrength  = (WORD)node.attribute("ReqStrength").as_int();
					si.RequireDexterity = (WORD)node.attribute("ReqDexterity").as_int();
					si.RequireEnergy    = (WORD)node.attribute("ReqEnergy").as_int();
					si.RequireVitality  = (WORD)node.attribute("ReqVitality").as_int();
					si.RequireCharisma  = (WORD)node.attribute("ReqCharisma").as_int();
				}

				if(secIndex == 13)
				{
					si.Resistance[0] = (BYTE)node.attribute("IceRes").as_int();
					si.Resistance[1] = (BYTE)node.attribute("PoisonRes").as_int();
					si.Resistance[2] = (BYTE)node.attribute("LightRes").as_int();
					si.Resistance[3] = (BYTE)node.attribute("FireRes").as_int();
					si.Resistance[4] = (BYTE)node.attribute("EarthRes").as_int();
					si.Resistance[5] = (BYTE)node.attribute("WindRes").as_int();
					si.Resistance[6] = (BYTE)node.attribute("WaterRes").as_int();
				}

				si.PowerATTK = (DWORD)node.attribute("ATTK").as_int();
			}

			for(int c=0;c<MAX_CLASS && c< (int)(sizeof(ClassNamesLong)/sizeof(ClassNamesLong[0]));c++)
			{
				if(!classFilter[c])
				{
					si.RequireClass[c] = 0;
				}
				else
				{
					pugi::xml_attribute attr = node.attribute(ClassNamesLong[c]);
					if(attr)
					{
						si.RequireClass[c] = (BYTE)attr.as_int();
					}
					else
					{
						si.RequireClass[c] = (BYTE)node.attribute(ClassNamesShort[c]).as_int();
					}
				}
			}

			int statType = GetSetItemStatType(setItemStatType,fullIndex);
			switch(statType)
			{
				case SetItemOptionLocal::ADD_STRENGTH:
					si.AttType = 1;
					break;
				case SetItemOptionLocal::ADD_DEXTERITY:
					si.AttType = 2;
					break;
				case SetItemOptionLocal::ADD_VITALITY:
					si.AttType = 4;
					break;
				case SetItemOptionLocal::ADD_ENERGY:
					si.AttType = 3;
					break;
				case SetItemOptionLocal::ADD_LEADERSHIP:
					si.AttType = 5;
					break;
				default:
					si.AttType = 0;
					break;
			}

			if(si.Kind1 == ItemKind1Local::GUARDIAN_MOUNT)
			{
				si.Durability = 0xFF;
			}

			items.push_back(si);
		}
	}

	if(items.empty())
	{
		MessageBoxW(0,L"В Item.xml не найдено ни одного <Item>",L"BMD Generator",MB_OK|MB_ICONWARNING);
		return false;
	}

	CreateDirectoryA("BMD",0);
	CreateDirectoryA("BMD\\Local",0);
	CreateDirectoryA("BMD\\Local\\Eng",0);

	const char* outPath = "BMD\\Local\\Eng\\Item.bmd";

	if(!PackFileEncryptLocal(outPath,(BYTE*)items.data(),(int)items.size(),sizeof(SCRIPT_ITEM_BMD),0xE2F1,true,true))
	{
		MessageBoxW(0,L"Не удалось записать Item.bmd",L"BMD Generator",MB_OK|MB_ICONERROR);
		return false;
	}

	return true;
}

int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
	int res = MessageBoxW(
		0,
		L"Сгенерировать BMD файлы (Item.bmd) из Item\\Item.xml?",
		L"BMD Generator",
		MB_YESNO | MB_ICONQUESTION
	);

	if(res != IDYES)
	{
		return 0;
	}

	if(GenerateItemBmd())
	{
		MessageBoxW(0,L"Готово.\nПроверьте BMD\\Local\\Eng\\Item.bmd",L"BMD Generator",MB_OK|MB_ICONINFORMATION);
	}

	return 0;
}

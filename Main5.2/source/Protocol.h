#pragma once
#include "WSclient.h"
#define SET_NUMBERHB(x) ((BYTE)((DWORD)(x)>>(DWORD)8))
#define SET_NUMBERLB(x) ((BYTE)((DWORD)(x)&0xFF))
#define SET_NUMBERHW(x) ((WORD)((DWORD)(x)>>(DWORD)16))
#define SET_NUMBERLW(x) ((WORD)((DWORD)(x)&0xFFFF))
#define SET_NUMBERHDW(x) ((DWORD)((QWORD)(x)>>(QWORD)32))
#define SET_NUMBERLDW(x) ((DWORD)((QWORD)(x)&0xFFFFFFFF))

#define MAKE_NUMBERW(x,y) ((WORD)(((BYTE)((y)&0xFF))|((BYTE)((x)&0xFF)<<8)))
#define MAKE_NUMBERDW(x,y) ((DWORD)(((WORD)((y)&0xFFFF))|((WORD)((x)&0xFFFF)<<16)))
#define MAKE_NUMBERQW(x,y) ((QWORD)(((DWORD)((y)&0xFFFFFFFF))|((DWORD)((x)&0xFFFFFFFF)<<32)))
//===Move Item
struct PMSG_ITEM_MOVE_RECV
{
	PSBMSG_HEAD h;
	BYTE sFlag;
	BYTE tFlag;
	BYTE Source;
	BYTE Target;
};
struct XULY_CGPACKET
{
	PSBMSG_HEAD header; // C3:F3:03
	DWORD ThaoTac;
};
struct SEND_COUNTLIST
{
	PSWMSG_HEAD header;
	int Count;
	BYTE Type;
};

struct PMSG_GM_ITEM_SPAWN_RECV
{
	PSBMSG_HEAD header; // C1:F3:F2
	BYTE action;
	BYTE itemIndex[2];
	BYTE level;
	BYTE skill;
	BYTE luck;
	BYTE option;
	BYTE exc;
	BYTE set;
	BYTE socket;
	WORD count;
};

struct PMSG_GM_CLEAR_INVENTORY_RECV
{
	PSBMSG_HEAD header; // C1:F3:F4
};

#pragma pack(push, 1)
struct PMSG_GM_MONSTER_DB_REQ
{
	PSBMSG_HEAD header; // C1:FA:00
	BYTE flags;
};

struct PMSG_GM_MONSTER_DB_BEGIN
{
	PSBMSG_HEAD header; // C1:FA:01
	WORD total;
	WORD chunkSize;
};

struct GM_MONSTER_INFO_NET
{
	int Index;
	int Rate;
	char Name[32];
	int Level;
	int AINumber;
	int ScriptLife;
	int Life;
	int Mana;
	int DamageMin;
	int DamageMax;
	int Defense;
	int MagicDefense;
	int AttackRate;
	int DefenseRate;
	int MoveRange;
	int AttackRange;
	int AttackType;
	int ViewRange;
	int MoveSpeed;
	int AttackSpeed;
	int RegenTime;
	int Attribute;
	int ItemRate;
	int MoneyRate;
	int MaxItemLevel;
	int Resistance[7];
	int MonsterSkill;
	int ElementalAttribute;
	int ElementalPattern;
	int ElementalDefense;
	int ElementalDamageMin;
	int ElementalDamageMax;
	int ElementalAttackRate;
	int ElementalDefenseRate;
};

struct PMSG_GM_MONSTER_DB_DATA
{
	PSWMSG_HEAD header; // C2:FA:02
	WORD start;
	WORD count;
	GM_MONSTER_INFO_NET items[1];
};

struct PMSG_GM_MONSTER_DB_END
{
	PSBMSG_HEAD header; // C1:FA:03
};
#pragma pack(pop)


struct RANK_INFO_SEND
{
	char NameRank[128];
};

extern std::vector <std::string> m_DataSelectNameTop;
BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int size, int key);

void GMMonsterDb_Reset();
bool GMMonsterDb_IsReady();
bool GMMonsterDb_IsLoading();
int GMMonsterDb_Total();
int GMMonsterDb_Received();
const std::vector<GM_MONSTER_INFO_NET>& GMMonsterDb_Get();

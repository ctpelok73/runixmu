#include "stdafx.h"
#include "Protocol.h"
#include "WSclient.h"
#include "NewUISystem.h"
#include "wsclientinline.h"
#include "SkillManager.h"
#include "CharacterManager.h"
#if (CB_GETMIXRATE)
#include "CB_GetMixRate.h"
#endif
#include "CustomEventTime.h"
#include "CB_DangKyInGame.h"

static bool s_gmMonsterDbLoading = false;
static bool s_gmMonsterDbReady = false;
static int s_gmMonsterDbTotal = 0;
static std::vector<GM_MONSTER_INFO_NET> s_gmMonsterDbItems;

void GMMonsterDb_Reset()
{
	s_gmMonsterDbLoading = false;
	s_gmMonsterDbReady = false;
	s_gmMonsterDbTotal = 0;
	s_gmMonsterDbItems.clear();
}

bool GMMonsterDb_IsReady()
{
	return s_gmMonsterDbReady;
}

bool GMMonsterDb_IsLoading()
{
	return s_gmMonsterDbLoading;
}

int GMMonsterDb_Total()
{
	return s_gmMonsterDbTotal;
}

int GMMonsterDb_Received()
{
	return (int)s_gmMonsterDbItems.size();
}

const std::vector<GM_MONSTER_INFO_NET>& GMMonsterDb_Get()
{
	return s_gmMonsterDbItems;
}

BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int size, int key) // OK
{
	switch (head)
	{
#if(CB_GETMIXRATE)
	case 0x88:
		if (gCB_GetMixRate) gCB_GetMixRate->GCRecvMixInfo(lpMsg, size);
		break;
#endif
	case 0xF3:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0xE8:
			g_CustomEventTime->GCReqEventTime((PMSG_CUSTOM_EVENTTIME_RECV*)lpMsg);
			return 1;
		}
		break;
	case 0xD3:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
#if(CB_DANGKYINGAME)
		case 0x05:
			gCB_DangKyInGame->RecvKQRegInGame((XULY_CGPACKET*)lpMsg);
			break;
#endif
		}
		break;
	case 0xFA:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0x01:
		{
			PMSG_GM_MONSTER_DB_BEGIN* p = (PMSG_GM_MONSTER_DB_BEGIN*)lpMsg;
			GMMonsterDb_Reset();
			s_gmMonsterDbLoading = true;
			s_gmMonsterDbTotal = (int)p->total;
			if (s_gmMonsterDbTotal > 0)
			{
				s_gmMonsterDbItems.reserve((size_t)s_gmMonsterDbTotal);
			}
		}
		return 1;
		case 0x02:
		{
			PMSG_GM_MONSTER_DB_DATA* p = (PMSG_GM_MONSTER_DB_DATA*)lpMsg;
			const int start = (int)p->start;
			const int count = (int)p->count;

			if (count <= 0)
				return 1;

			if (start == (int)s_gmMonsterDbItems.size())
			{
				for (int i = 0; i < count; ++i)
				{
					s_gmMonsterDbItems.push_back(p->items[i]);
				}
			}
			else
			{
				const int need = start + count;
				if (need > (int)s_gmMonsterDbItems.size())
				{
					s_gmMonsterDbItems.resize(need);
				}
				for (int i = 0; i < count; ++i)
				{
					s_gmMonsterDbItems[start + i] = p->items[i];
				}
			}
		}
		return 1;
		case 0x03:
		{
			s_gmMonsterDbLoading = false;
			if (s_gmMonsterDbTotal <= 0 || (int)s_gmMonsterDbItems.size() >= s_gmMonsterDbTotal)
			{
				s_gmMonsterDbReady = true;
			}
		}
		return 1;
		}
		break;

	}
	return false;
}

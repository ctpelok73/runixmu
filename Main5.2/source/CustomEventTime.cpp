#include "stdafx.h"
#include "CustomEventTime.h"
#include "CBInterface.h"
#include "Util.h"
#include "TextClien.h"
#include "NewUISystem.h"
#include "UIControls.h"

#include "wsclientinline.h"

using namespace SEASON3B;


CCustomEventTime* CCustomEventTime::Instance()
{
	static CCustomEventTime s_Instance;
	return &s_Instance;
}


CCustomEventTime::CCustomEventTime()
{
}

void CCustomEventTime::Init()
{
	this->Click = false;
	this->mNewDataEventTime.clear();
	this->MaxListData = 0;
	this->m_HoverRow = -1;
}


void CCustomEventTime::Load(CUSTOM_EVENT_INFO* info) // OK
{
	for (int n = 0; n < MAX_EVENTTIME; n++)
	{
		this->SetInfo(info[n]);
	}
}

void CCustomEventTime::SetInfo(CUSTOM_EVENT_INFO info) // OK
{
	if (info.Index < 0 || info.Index >= MAX_EVENTTIME)
	{
		return;
	}

	this->m_CustomEventInfo[info.Index] = info;
}

void CCustomEventTime::ClearCustomEventTime() // OK
{
	for (int n = 0; n < MAX_EVENTTIME; n++)
	{
		gCustomEventTime[n].time = -1;
	}
	this->count = 0;
	this->EventTimeEnable = 0;
}
void CCustomEventTime::OpenTestWindow(int Page)
{
	gInterface->Data[eWindowEventTime].OnShow = true;

	PMSG_CUSTOM_EVENTTIME_SEND pMsg;

	pMsg.header.set(0xF3, 0xE8, sizeof(pMsg));

	pMsg.Page = Page;

	DataSend((BYTE*)&pMsg, pMsg.header.size);

};
void CCustomEventTime::OnOffWindow() // OK
{
	gInterface->Data[eWindowEventTime].OpenClose();
	if (gInterface->Data[eWindowEventTime].OnShow)
	{
		g_CustomEventTime->ClearCustomEventTime();
		g_CustomEventTime->OpenTestWindow();
	}
}
void CCustomEventTime::GCReqEventTime(PMSG_CUSTOM_EVENTTIME_RECV* lpMsg) // OK
{
	this->count = lpMsg->count;
	this->mNewDataEventTime.clear();
	this->MaxListData = lpMsg->MaxList;
	for (int n = 0; n < lpMsg->count; n++)
	{
		CUSTOM_EVENTTIME_DATA* lpInfo = (CUSTOM_EVENTTIME_DATA*)(((BYTE*)lpMsg) + sizeof(PMSG_CUSTOM_EVENTTIME_RECV) + (sizeof(CUSTOM_EVENTTIME_DATA) * n));

		//this->gCustomEventTime[n].index = lpInfo->index;
		//this->gCustomEventTime[n].time = lpInfo->time;
		//g_Console.AddMessage(1, "Debug %d (%d) %s Max %d", lpInfo->index, lpInfo->time, lpInfo->NameEvent, this->MaxListData);
		this->mNewDataEventTime.push_back(*lpInfo);
		//if (this->gCustomEventTime[n].index >= 28 && this->gCustomEventTime[n].time != -1) this->Arena = 1;
	}

	this->EventTimeEnable = 1;
}

void CCustomEventTime::DrawEventTimePanelWindow()
{

	if (!gInterface->Data[eWindowEventTime].OnShow)
	{
		return;
	}


	float MainWidth = 460;
	float MainHeight = 380;
	float StartY = 60.0;
	float StartX = (MAX_WIN_WIDTH / 2) - (MainWidth / 2);

	gInterface->gDrawWindowCustom(&StartX, &StartY, MainWidth, MainHeight, eWindowEventTime, "Bảng Thời Gian Sự Kiện");

	float ColWidth = (MainWidth - 40) / 4.0f;
	float ContentX = StartX + 15;
	float ButtonX = StartX + (MainWidth / 2) - (29.0f / 2);
	float StartBody = StartY;
	DWORD Color = 0xFFFFFFB8;

	// --- Pagination info ---
	TextDraw((HFONT)g_hFont, StartX, StartY + (MainHeight - 38), 0x7DF4FFFF, 0x0, (int)MainWidth, 0, 3, "Trang: %d/%d", this->Page + 1, (this->MaxListData / 14) + 1);

	if (this->Page > 0)
	{
		if (gInterface->DrawButtonGUI(CNewUIInGameShop::IMAGE_IGS_PAGE_LEFT, StartX + 50, StartY + (MainHeight - 43), 20, 23, 3))
		{
			this->Page--;
			this->OpenTestWindow(this->Page);
		}
	}
	if ((this->MaxListData / 14) > this->Page)
	{
		if (gInterface->DrawButtonGUI(CNewUIInGameShop::IMAGE_IGS_PAGE_RIGHT, StartX + (MainWidth - 75), StartY + (MainHeight - 43), 20, 23, 3))
		{
			this->Page++;
			this->OpenTestWindow(this->Page);
		}
	}

	if (this->EventTimeEnable == 1)
	{
		// --- Header background bar ---
		gInterface->DrawBarForm(ContentX - 5, StartBody + 33, MainWidth - 20, 18, 0.15f, 0.18f, 0.25f, 0.85f);

		// --- Header separator line (bottom of header) ---
		gInterface->DrawBarForm(ContentX - 5, StartBody + 51, MainWidth - 20, 1, 0.5f, 0.55f, 0.6f, 0.6f);

		// --- Column headers ---
		TextDraw(g_hFontBold, ContentX, StartBody + 36, 0xFFFFFFA8, 0x0, (int)ColWidth + 20, 0, 3, "Sự kiện");
		TextDraw(g_hFontBold, ContentX + ColWidth + 20, StartBody + 36, 0xFFFFFFA8, 0x0, (int)ColWidth, 0, 3, "Thời Gian");
		TextDraw(g_hFontBold, ContentX + (ColWidth * 2) + 20, StartBody + 36, 0xFFFFFFA8, 0x0, (int)ColWidth, 0, 3, "Trạng thái");
		TextDraw(g_hFontBold, ContentX + (ColWidth * 3) + 10, StartBody + 36, 0xFFFFFFA8, 0x0, (int)ColWidth, 0, 3, "Move");

		// --- Timer countdown (1 second tick) ---
		if ((GetTickCount() - this->EventTimeTickCount) > 1000)
		{
			for (int i = 0; i < this->count; i++)
			{
				if (this->mNewDataEventTime[i].time > 0)
				{
					this->mNewDataEventTime[i].time = this->mNewDataEventTime[i].time - 1;
				}
			}
			this->EventTimeTickCount = GetTickCount();
		}

		char text2[30];
		int totalseconds;
		int hours;
		int minutes;
		int seconds;
		int days;

		int line = 0;
		int rowHeight = 18;
		this->m_HoverRow = -1;

		for (size_t i = 0; i < this->mNewDataEventTime.size(); i++)
		{
			float rowY = StartBody + 55 + (line);

			// --- Alternating row background (zebra striping) ---
			if (i % 2 == 0)
			{
				gInterface->DrawBarForm(ContentX - 5, rowY - 1, MainWidth - 20, (float)rowHeight, 0.2f, 0.22f, 0.28f, 0.4f);
			}
			else
			{
				gInterface->DrawBarForm(ContentX - 5, rowY - 1, MainWidth - 20, (float)rowHeight, 0.15f, 0.17f, 0.22f, 0.3f);
			}

			// --- Hover highlight ---
			if (SEASON3B::CheckMouseIn(ContentX - 5, rowY - 1, MainWidth - 20, (float)rowHeight) == 1)
			{
				gInterface->DrawBarForm(ContentX - 5, rowY - 1, MainWidth - 20, (float)rowHeight, 0.3f, 0.5f, 0.8f, 0.25f);
				this->m_HoverRow = (int)i;
			}

			// --- Format time string ---
			if (this->mNewDataEventTime[i].time <= -1)
			{
				wsprintf(text2, "Disabled");
			}
			else if (this->mNewDataEventTime[i].time == 0)
			{
				wsprintf(text2, "Online");
			}
			else
			{
				totalseconds = this->mNewDataEventTime[i].time;
				hours = totalseconds / 3600;
				minutes = (totalseconds / 60) % 60;
				seconds = totalseconds % 60;

				if (hours > 23)
				{
					days = hours / 24;
					wsprintf(text2, "%d day(s)+", days);
				}
				else
				{
					wsprintf(text2, "%02d:%02d:%02d", hours, minutes, seconds);
				}
			}

			// --- Color coding for status ---
			if (this->mNewDataEventTime[i].time <= -1)
			{
				// Disabled: muted gray
				Color = 0x888888B8;
			}
			else if (this->mNewDataEventTime[i].time == 0)
			{
				// Online: bright green
				Color = 0x00FF80FF;
			}
			else if (this->mNewDataEventTime[i].time < 300)
			{
				// Soon (< 5 min): orange-yellow
				Color = 0xFFA500FF;
			}
			else
			{
				// Normal: white
				Color = 0xFFFFFFB8;
			}

			// --- Event name (bold, slightly pink-red tint like original) ---
			TextDraw(g_hFontBold, ContentX, rowY + 2, 0xFFFF478A, 0x0, (int)ColWidth + 20, 0, 3, TEXT(this->mNewDataEventTime[i].NameEvent));

			// --- Time column ---
			TextDraw(g_hFontBold, ContentX + ColWidth + 20, rowY + 2, Color, 0x0, (int)ColWidth, 0, 3, TEXT(text2));

			// --- Status / Description column ---
			DWORD descColor = 0x61FFD0A8;
			if (this->mNewDataEventTime[i].time == 0)
			{
				descColor = 0x00FF80FF;
			}
			else if (this->mNewDataEventTime[i].time <= -1)
			{
				descColor = 0x888888B8;
			}
			TextDraw(g_hFontBold, ContentX + (ColWidth * 2) + 20, rowY + 2, descColor, 0x0, (int)ColWidth, 0, 3, TEXT(this->mNewDataEventTime[i].DesString));

			// --- Move button ---
			if (this->mNewDataEventTime[i].NumberGate != -1)
			{
				float moveX = ContentX + (ColWidth * 3) + 25;
				if (gInterface->DrawButtonGUI(BITMAP_HERO_POSITION_INFO_BEGIN + 6, moveX, rowY + 2, 18, 13))
				{
					XULY_CGPACKET pMsg;
					pMsg.header.set(0xD3, 0x01, sizeof(pMsg));
					pMsg.ThaoTac = this->mNewDataEventTime[i].NumberGate;
					DataSend((LPBYTE)&pMsg, pMsg.header.size);
				}

				if (SEASON3B::CheckMouseIn(moveX, rowY + 2, 18, 13) == 1)
				{
					RenderTipText((int)(moveX + 30), (int)(rowY + 2), "Di Chuyển Đến Event");
				}
			}

			line += rowHeight;
		}

		// --- Bottom separator line ---
		float bottomLineY = StartBody + 55 + line + 2;
		gInterface->DrawBarForm(ContentX - 5, bottomLineY, MainWidth - 20, 1, 0.4f, 0.45f, 0.5f, 0.5f);
	}
	else
	{
		if (this->EventTimeLoad == 1)
		{
			gInterface->DrawFormat(eGold, (int)(StartX + MainWidth / 2 - 30), (int)(StartBody + 100), 100, 1, "Loading ..");
			this->EventTimeLoad = 2;
		}
		else if (this->EventTimeLoad == 2)
		{
			gInterface->DrawFormat(eGold, (int)(StartX + MainWidth / 2 - 30), (int)(StartBody + 100), 100, 1, "Loading ...");
			this->EventTimeLoad = 3;
		}
		else if (this->EventTimeLoad == 3)
		{
			gInterface->DrawFormat(eGold, (int)(StartX + MainWidth / 2 - 30), (int)(StartBody + 100), 100, 1, "Loading ....");
			this->EventTimeLoad = 4;
		}
		else if (this->EventTimeLoad == 4)
		{
			gInterface->DrawFormat(eGold, (int)(StartX + MainWidth / 2 - 30), (int)(StartBody + 100), 100, 1, "Loading .....");
			this->EventTimeLoad = 0;
		}
		else
		{
			gInterface->DrawFormat(eGold, (int)(StartX + MainWidth / 2 - 30), (int)(StartBody + 100), 100, 1, "Loading .");
			this->EventTimeLoad = 1;
		}
	}

}

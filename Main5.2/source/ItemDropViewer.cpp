#include "stdafx.h"
#include "ItemDropViewer.h"
#include "NewUIItemTooltip.h"
#include "NewUISystem.h"
#include "ZzzInventory.h"
#include "ZzzInfomation.h"

void CItemDropViewer::Render(void* item, int x, int y)
{
	if (item == NULL) return;

	ITEM_t* pItemMap = (ITEM_t*)item;

	if (pItemMap->Item.Type == ITEM_POTION + 15) return;
	
	// Используем существующую систему тултипов для отрисовки
	// Мы передаем pItemMap->Item, так как RenderItemTooltip ожидает ITEM*
	// x, y - координаты для отрисовки (обычно координаты мыши или предмета)
	
	if (g_pNewItemTooltip)
	{
		ITEM tempItem = pItemMap->Item;
		ItemConvert(&tempItem, pItemMap->Item.Level, pItemMap->Item.Option1, pItemMap->Item.ExtOption);
		// RenderItemTooltip(sx, sy, ITEM* pItem, bool Sell, int InvenType, bool bItemTextListBoxUse, bool bRender3d)
		g_pNewItemTooltip->RenderItemTooltip(x, y, &tempItem, false, 0, false, true);
	}
}

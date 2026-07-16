/**
 ******************************************************************************
 * @file    bsp_panel.c
 * @brief   基于ILI9341+XPT2046的测量项目选择控制面板
 * @note    依赖已有的 bsp_ili9341_lcd / bsp_xpt2046_lcd 驱动
 ******************************************************************************
 */

#include "bsp_panel.h"
#include "bsp_ili9341_lcd.h"
#include <string.h>

/******************************* 按钮与页面结构定义 ***************************/
typedef struct
{
    uint16_t     x;
    uint16_t     y;
    uint16_t     w;
    uint16_t     h;
    char        *label;
    MEAS_ITEM_t  item;      /* 命中该按钮时对应的动作/测量项目 */
} PanelButton_t;

typedef enum
{
    PAGE_MAIN = 0,
    PAGE_FAULT_SUB,
} PanelPage_t;

/* 用两个不与 MEAS_ITEM_t 正常取值冲突的哨兵值表示"翻页"类按钮 */
#define ACTION_ENTER_FAULT_SUB   ((MEAS_ITEM_t)0xFE)
#define ACTION_BACK_MAIN         ((MEAS_ITEM_t)0xFD)

/******************************* 颜色定义(复用bsp_ili9341_lcd.h里的宏) ***************************/
#define PANEL_BTN_COLOR             BLUE2
#define PANEL_BTN_SELECTED_COLOR    GREEN
#define PANEL_BTN_BORDER_COLOR      WHITE
#define PANEL_BTN_TEXT_COLOR        WHITE
#define PANEL_BG_COLOR              BLACK
#define PANEL_TITLE_COLOR           YELLOW

/******************************* 主菜单按钮布局 ***************************
 * 假设屏幕为 240(X) * 320(Y) 纵向使用，按钮宽200高40，垂直排布
 * 如实际屏幕方向/尺寸不同，请调整下面的坐标                                */
static const PanelButton_t s_MainMenu[] =
{
    { 20,  40, 200, 40, "1.Input R  ",   MEAS_INPUT_R           },
    { 20,  95, 200, 40, "2.Output R ",   MEAS_OUTPUT_R          },
    { 20, 150, 200, 40, "3.Gain     ",   MEAS_GAIN              },
    { 20, 205, 200, 40, "4.Freq Resp",   MEAS_FREQ_RESP         },
    { 20, 260, 200, 40, "5.Fault >> ",   ACTION_ENTER_FAULT_SUB },
};
#define MAIN_MENU_COUNT   ( sizeof(s_MainMenu) / sizeof(s_MainMenu[0]) )

/******************************* 故障检测子菜单按钮布局 ***************************/
static const PanelButton_t s_FaultSubMenu[] =
{
    { 20,  40,  90, 40, "R1",   MEAS_FAULT_R1 },
    {130,  40,  90, 40, "R2",   MEAS_FAULT_R2 },
    { 20,  90,  90, 40, "R3",   MEAS_FAULT_R3 },
    {130,  90,  90, 40, "R4",   MEAS_FAULT_R4 },
    { 20, 140,  90, 40, "C1",   MEAS_FAULT_C1 },
    {130, 140,  90, 40, "C2",   MEAS_FAULT_C2 },
    { 20, 190,  90, 40, "C3",   MEAS_FAULT_C3 },
    {130, 190,  90, 40, "Back", ACTION_BACK_MAIN },
};
#define FAULT_SUB_COUNT   ( sizeof(s_FaultSubMenu) / sizeof(s_FaultSubMenu[0]) )

/******************************* 内部状态 ***************************/
static PanelPage_t         s_CurrentPage      = PAGE_MAIN;
static MEAS_ITEM_t         s_SelectedItem     = MEAS_NONE;
static volatile uint8_t    s_NewSelectionFlag = 0;

/**
 * @brief  绘制单个按钮(底色+边框+居中文字)
 */
static void Panel_DrawButton(const PanelButton_t *btn, uint8_t selected)
{
    uint16_t textX, textY, textLenPx;

    /* 按钮底色 */
    LCD_SetTextColor(selected ? PANEL_BTN_SELECTED_COLOR : PANEL_BTN_COLOR);
    ILI9341_DrawRectangle(btn->x, btn->y, btn->w, btn->h, 1);

    /* 按钮边框(不填充) */
    LCD_SetTextColor(PANEL_BTN_BORDER_COLOR);
    ILI9341_DrawRectangle(btn->x, btn->y, btn->w, btn->h, 0);

    /* 按钮文字，粗略按8像素/字符估算居中位置(英文字体，如换成中文标签需按16px/字重新估算) */
    LCD_SetTextColor(PANEL_BTN_TEXT_COLOR);
    LCD_SetBackColor(selected ? PANEL_BTN_SELECTED_COLOR : PANEL_BTN_COLOR);

    textLenPx = (uint16_t)(strlen(btn->label) * 8);
    textX = btn->x + (btn->w > textLenPx ? (btn->w - textLenPx) / 2 : 4);
    textY = btn->y + (btn->h > 16 ? (btn->h - 16) / 2 : 0);

    ILI9341_DispString_EN(textX, textY, btn->label);
}

static void Panel_DrawTitle(const char *title)
{
    LCD_SetTextColor(PANEL_TITLE_COLOR);
    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_DispString_EN(20, 5, (char *)title);
}

/**
 * @brief  重绘整个页面(标题+对应页面的所有按钮)
 * @note   每次触摸后整屏重绘会有轻微闪烁，如需优化可只重绘发生变化的按钮
 */
static void Panel_DrawPage(PanelPage_t page)
{
    uint8_t i;

    ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

    if (page == PAGE_MAIN)
    {
        Panel_DrawTitle("Select Measurement Item");
        for (i = 0; i < MAIN_MENU_COUNT; i++)
        {
            uint8_t sel = (s_MainMenu[i].item == s_SelectedItem);
            Panel_DrawButton(&s_MainMenu[i], sel);
        }
    }
    else /* PAGE_FAULT_SUB */
    {
        Panel_DrawTitle("Fault Detect - Select Part");
        for (i = 0; i < FAULT_SUB_COUNT; i++)
        {
            uint8_t sel = (s_FaultSubMenu[i].item == s_SelectedItem);
            Panel_DrawButton(&s_FaultSubMenu[i], sel);
        }
    }
}

/**
 * @brief  判断触摸坐标是否落在按钮矩形内
 */
static uint8_t Panel_HitTest(const PanelButton_t *btn, uint16_t usX, uint16_t usY)
{
    return (usX >= btn->x) && (usX <= (btn->x + btn->w)) &&
           (usY >= btn->y) && (usY <= (btn->y + btn->h));
}

/**
 * @brief  控制面板初始化，绘制主菜单
 * @note   请在 ILI9341_Init() 和 XPT2046_Init() 之后调用
 */
void Panel_Init(void)
{
    s_CurrentPage      = PAGE_MAIN;
    s_SelectedItem     = MEAS_NONE;
    s_NewSelectionFlag = 0;
    Panel_DrawPage(s_CurrentPage);
}

/**
 * @brief  处理一次触摸坐标命中判断
 * @param  usX, usY : 触摸点在LCD上的坐标(已经过XPT2046触摸校准转换)
 * @note   建议在 XPT2046_TouchDown() 回调中调用本函数，
 *         即用 Panel_HandleTouch(touch->x, touch->y) 替换/追加
 *         原 THD_Display_TouchDown(touch->x, touch->y) 调用
 */
void Panel_HandleTouch(uint16_t usX, uint16_t usY)
{
    uint8_t i;

    if (s_CurrentPage == PAGE_MAIN)
    {
        for (i = 0; i < MAIN_MENU_COUNT; i++)
        {
            if (Panel_HitTest(&s_MainMenu[i], usX, usY))
            {
                if (s_MainMenu[i].item == ACTION_ENTER_FAULT_SUB)
                {
                    s_CurrentPage = PAGE_FAULT_SUB;
                    Panel_DrawPage(s_CurrentPage);
                }
                else
                {
                    s_SelectedItem     = s_MainMenu[i].item;
                    s_NewSelectionFlag = 1;
                    Panel_DrawPage(s_CurrentPage);   /* 重绘以高亮选中按钮 */
                }
                return;   /* 命中即返回，避免同一次触摸命中多个按钮 */
            }
        }
    }
    else /* PAGE_FAULT_SUB */
    {
        for (i = 0; i < FAULT_SUB_COUNT; i++)
        {
            if (Panel_HitTest(&s_FaultSubMenu[i], usX, usY))
            {
                if (s_FaultSubMenu[i].item == ACTION_BACK_MAIN)
                {
                    s_CurrentPage = PAGE_MAIN;
                    Panel_DrawPage(s_CurrentPage);
                }
                else
                {
                    s_SelectedItem     = s_FaultSubMenu[i].item;
                    s_NewSelectionFlag = 1;
                    Panel_DrawPage(s_CurrentPage);
                }
                return;
            }
        }
    }
}

/**
 * @brief  查看当前选中的测量项目(不清除"有新选择"标志)
 */
MEAS_ITEM_t Panel_GetSelectedItem(void)
{
    return s_SelectedItem;
}

/**
 * @brief  是否存在尚未被主循环处理的新选择
 */
uint8_t Panel_IsNewSelectionAvailable(void)
{
    return s_NewSelectionFlag;
}

/**
 * @brief  取出当前选择并清除"新选择"标志(高亮显示保留，避免重复触发测量流程)
 * @retval 当前选中的测量项目；如无新选择也会返回当前值，请配合
 *         Panel_IsNewSelectionAvailable() 判断是否要执行动作
 */
MEAS_ITEM_t Panel_FetchSelection(void)
{
    s_NewSelectionFlag = 0;
    return s_SelectedItem;
}

/**
 * @brief  清除选中状态(取消按钮高亮)，如在一次测量流程结束后想恢复未选中外观可调用
 */
void Panel_ClearSelection(void)
{
    s_SelectedItem = MEAS_NONE;
    Panel_DrawPage(s_CurrentPage);
}

/*********************end of file*************************/
/**
 ******************************************************************************
 * @file    bsp_panel.c
 * @brief   ILI9341+XPT2046 控制面板 —— 横向布局，双模式 + Debug面板 + 幅频曲线
 * @note    屏幕方向：横向 320(X)×240(Y)，ILI9341_GramScan(3)
 *
 * 布局（横向 320×240）：
 *   ┌──────────────────────────────────────────────────────┐
 *   │  状态栏(0~21): 模式名称 + 模式切换提示                │
 *   ├──────────────┬───────────────────────────────────────┤
 *   │  按钮区       │   结果 / Debug / 幅频曲线绘图区        │
 *   │  (0~139)     │   (140~319)                           │
 *   │  22~239      │   22~239                              │
 *   └──────────────┴───────────────────────────────────────┘
 ******************************************************************************
 */

#include "bsp_panel.h"
#include "bsp_ili9341_lcd.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/******************************* 按钮定义 ***************************/
typedef struct
{
    uint16_t      x, y, w, h;
    const char   *label;
    PanelAction_t action;
} PanelButton_t;

/******************************* 颜色定义 ***************************/
#define PANEL_BTN_COLOR           BLUE2
#define PANEL_BTN_SELECTED_COLOR  GREEN
#define PANEL_BTN_BORDER_COLOR    WHITE
#define PANEL_BTN_TEXT_COLOR      WHITE
#define PANEL_BG_COLOR            BLACK
#define PANEL_RESULT_TEXT_COLOR   CYAN
#define PANEL_DEBUG_TEXT_COLOR    YELLOW
#define PANEL_GRAPH_AXIS_COLOR    GREY
#define PANEL_GRAPH_LINE_COLOR    GREEN
#define PANEL_GRAPH_CUTOFF_COLOR  RED
#define PANEL_STATUSBAR_BG        BLUE
#define PANEL_STATUSBAR_TEXT      WHITE

/******************************* 布局常量（横向 320×240） ***************************/
#define STATUSBAR_H        22
#define BTN_AREA_X         0
#define BTN_AREA_Y         STATUSBAR_H
#define BTN_AREA_W         140
#define BTN_AREA_H         (240 - STATUSBAR_H)

#define RIGHT_AREA_X       140
#define RIGHT_AREA_Y       STATUSBAR_H
#define RIGHT_AREA_W       180
#define RIGHT_AREA_H       (240 - STATUSBAR_H)

#define RESULT_LINE_H      16

/* 按钮布局 */
#define BTN_X              4
#define BTN_W              132
#define BTN_H              32
#define BTN_GAP            5
#define BTN_TOP_Y          (BTN_AREA_Y + 4)

/* 幅频曲线绘图区（在右侧结果区的下半部分） */
#define GRAPH_X            (RIGHT_AREA_X + 8)
#define GRAPH_Y            (RIGHT_AREA_Y + 136)
#define GRAPH_W            160
#define GRAPH_H            74

/* Debug 区（结果区下方，曲线图上方） */
#define DEBUG_AREA_Y        (RIGHT_AREA_Y + 90)
#define DEBUG_AREA_H        42
#define DEBUG_LINE_H        14
#define DEBUG_MAX_LINES     (DEBUG_AREA_H / DEBUG_LINE_H)   /* 3 lines */

/******************************* Mode1 测量模式按钮 ***************************/
static const PanelButton_t s_btnMeasure[] =
{
    { BTN_X, BTN_TOP_Y,                       BTN_W, BTN_H, "Measure All",   ACTION_MEASURE_ALL  },
    { BTN_X, BTN_TOP_Y + (BTN_H+BTN_GAP),     BTN_W, BTN_H, "Input R",       ACTION_INPUT_R      },
    { BTN_X, BTN_TOP_Y + 2*(BTN_H+BTN_GAP),   BTN_W, BTN_H, "Output R",      ACTION_OUTPUT_R     },
    { BTN_X, BTN_TOP_Y + 3*(BTN_H+BTN_GAP),   BTN_W, BTN_H, "Gain @1kHz",    ACTION_GAIN_1K      },
    { BTN_X, BTN_TOP_Y + 4*(BTN_H+BTN_GAP),   BTN_W, BTN_H, "Freq Response", ACTION_FREQ_RESP    },
};
#define MEASURE_BTN_COUNT (sizeof(s_btnMeasure)/sizeof(s_btnMeasure[0]))

/******************************* Mode2 故障检测模式按钮 ***************************/
static const PanelButton_t s_btnFault[] =
{
    { BTN_X, BTN_TOP_Y,                   BTN_W, BTN_H, "Detect Fault",  ACTION_FAULT_DETECT },
};
#define FAULT_BTN_COUNT (sizeof(s_btnFault)/sizeof(s_btnFault[0]))

/******************************* 内部状态 ***************************/
static PanelMode_t      s_mode           = MODE_MEASURE;
static PanelAction_t    s_selectedAction = ACTION_NONE;
static volatile uint8_t s_newActionFlag  = 0;

/* Debug 缓冲：存储调试信息行 */
static char   s_debugLines[DEBUG_MAX_LINES][40];
static uint8_t s_debugCount = 0;

/* 上次绘制的幅频数据缓存（用于模式切换后恢复） */
static FreqRespPoint s_lastFreqResp[FREQRESP_POINT_NUM];
static float         s_lastMidGain = 0.0f;
static float         s_lastCutoff  = 0.0f;
static uint8_t       s_hasFreqData = 0;

/******************************* 内部函数声明 ***************************/
static void Panel_DrawStatusBar(void);
static void Panel_DrawButton(const PanelButton_t *btn, uint8_t selected);
static void Panel_DrawButtonArea(void);
static void Panel_ClearRightArea(void);
static uint8_t Panel_HitTest(const PanelButton_t *btn, uint16_t x, uint16_t y);

/* ======================== 绘图辅助 ======================== */

/**
 * @brief  绘制幅频特性曲线（右侧下半区域）
 * @param  points   : 频率响应数据点数组
 * @param  count    : 数据点个数
 * @param  mid_gain_db : 中频参考增益(dB)，用于绘制 -3dB 参考线
 * @param  cutoff_freq : 截止频率(Hz)
 */
void Panel_DrawFreqResp(const FreqRespPoint *points, uint8_t count,
                        float mid_gain_db, float cutoff_freq)
{
    uint8_t i;
    float   f_min, f_max, g_min, g_max;
    int16_t px0, py0, px1, py1;

    if (count < 2) return;

    /* 保存数据供后续重绘 */
    for (i = 0; i < count && i < FREQRESP_POINT_NUM; i++)
        s_lastFreqResp[i] = points[i];
    s_lastMidGain  = mid_gain_db;
    s_lastCutoff   = cutoff_freq;
    s_hasFreqData  = 1;

    /* 清除绘图区域 */
    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(GRAPH_X - 2, GRAPH_Y - 2, GRAPH_W + 6, GRAPH_H + 6);

    /* 绘制坐标轴 */
    LCD_SetTextColor(PANEL_GRAPH_AXIS_COLOR);
    ILI9341_DrawLine(GRAPH_X, GRAPH_Y, GRAPH_X, GRAPH_Y + GRAPH_H);
    ILI9341_DrawLine(GRAPH_X, GRAPH_Y + GRAPH_H, GRAPH_X + GRAPH_W, GRAPH_Y + GRAPH_H);

    /* 计算范围 */
    f_min = log10f(points[0].freq_hz);
    f_max = log10f(points[count - 1].freq_hz);
    g_min = points[0].gain_db;
    g_max = points[0].gain_db;
    for (i = 1; i < count; i++)
    {
        if (points[i].gain_db < g_min) g_min = points[i].gain_db;
        if (points[i].gain_db > g_max) g_max = points[i].gain_db;
    }
    /* 给增益范围加些边距 */
    {
        float margin = (g_max - g_min) * 0.15f;
        if (margin < 2.0f) margin = 2.0f;
        g_min -= margin;
        g_max += margin;
    }

    /* -3dB 参考虚线（如果 mid_gain_db - 3dB 在范围内） */
    {
        float g_3db = mid_gain_db - 3.0f;
        if (g_3db >= g_min && g_3db <= g_max)
        {
            int16_t y3 = GRAPH_Y + (int16_t)((g_max - g_3db) / (g_max - g_min) * GRAPH_H);
            LCD_SetTextColor(PANEL_GRAPH_CUTOFF_COLOR);
            /* 画虚线：每 4 像素画一点 */
            for (i = 0; i < GRAPH_W; i += 5)
                ILI9341_SetPointPixel(GRAPH_X + i, y3);
        }
    }

    /* 绘制曲线（折线连接各点） */
    LCD_SetTextColor(PANEL_GRAPH_LINE_COLOR);
    for (i = 0; i < count; i++)
    {
        px1 = GRAPH_X + (int16_t)((log10f(points[i].freq_hz) - f_min) / (f_max - f_min) * GRAPH_W);
        py1 = GRAPH_Y + (int16_t)((g_max - points[i].gain_db) / (g_max - g_min) * GRAPH_H);

        /* 画数据点（小圆圈，半径2） */
        ILI9341_DrawCircle(px1, py1, 2, 1);

        if (i > 0)
        {
            ILI9341_DrawLine(px0, py0, px1, py1);
        }
        px0 = px1;
        py0 = py1;
    }

    /* 绘制坐标轴标签 */
    {
        char buf[16];
        LCD_SetTextColor(PANEL_GRAPH_AXIS_COLOR);
        LCD_SetBackColor(PANEL_BG_COLOR);
        snprintf(buf, sizeof(buf), "%.0fHz", points[0].freq_hz);
        ILI9341_DispString_EN(GRAPH_X, GRAPH_Y + GRAPH_H + 2, buf);
        snprintf(buf, sizeof(buf), "%.0fk", points[count-1].freq_hz/1000.0f);
        ILI9341_DispString_EN(GRAPH_X + GRAPH_W - 28, GRAPH_Y + GRAPH_H + 2, buf);
        snprintf(buf, sizeof(buf), "%.0fdB", g_max);
        ILI9341_DispString_EN(GRAPH_X - 32, GRAPH_Y, buf);
    }
}

/* ======================== 内部实现 ======================== */

static void Panel_DrawStatusBar(void)
{
    LCD_SetTextColor(PANEL_STATUSBAR_BG);
    ILI9341_DrawRectangle(0, 0, LCD_X_LENGTH, STATUSBAR_H, 1);

    LCD_SetTextColor(PANEL_STATUSBAR_TEXT);
    LCD_SetBackColor(PANEL_STATUSBAR_BG);

    if (s_mode == MODE_MEASURE)
    {
        ILI9341_DispString_EN(4, 3, "Mode1:Measure  [KEY->Fault]");
    }
    else
    {
        ILI9341_DispString_EN(4, 3, "Mode2:FaultDet  [KEY->Measure]");
    }
}

static void Panel_DrawButton(const PanelButton_t *btn, uint8_t selected)
{
    uint16_t textX, textY, textLenPx;

    LCD_SetTextColor(selected ? PANEL_BTN_SELECTED_COLOR : PANEL_BTN_COLOR);
    ILI9341_DrawRectangle(btn->x, btn->y, btn->w, btn->h, 1);

    LCD_SetTextColor(PANEL_BTN_BORDER_COLOR);
    ILI9341_DrawRectangle(btn->x, btn->y, btn->w, btn->h, 0);

    LCD_SetTextColor(PANEL_BTN_TEXT_COLOR);
    LCD_SetBackColor(selected ? PANEL_BTN_SELECTED_COLOR : PANEL_BTN_COLOR);

    textLenPx = (uint16_t)(strlen(btn->label) * 8);
    textX = btn->x + (btn->w > textLenPx ? (btn->w - textLenPx) / 2 : 2);
    textY = btn->y + (btn->h > 16 ? (btn->h - 16) / 2 : 0);

    ILI9341_DispString_EN(textX, textY, (char *)btn->label);
}

static void Panel_DrawButtonArea(void)
{
    uint8_t i, count;
    const PanelButton_t *btns;

    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(BTN_AREA_X, BTN_AREA_Y, BTN_AREA_W, BTN_AREA_H);

    if (s_mode == MODE_MEASURE)
    {
        btns  = s_btnMeasure;
        count = MEASURE_BTN_COUNT;
    }
    else
    {
        btns  = s_btnFault;
        count = FAULT_BTN_COUNT;
    }

    for (i = 0; i < count; i++)
    {
        uint8_t sel = (btns[i].action == s_selectedAction);
        Panel_DrawButton(&btns[i], sel);
    }
}

static void Panel_ClearRightArea(void)
{
    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(RIGHT_AREA_X, RIGHT_AREA_Y, RIGHT_AREA_W, RIGHT_AREA_H);
}

static uint8_t Panel_HitTest(const PanelButton_t *btn, uint16_t x, uint16_t y)
{
    return (x >= btn->x) && (x <= (btn->x + btn->w)) &&
           (y >= btn->y) && (y <= (btn->y + btn->h));
}

/* ======================== 公开接口 ======================== */

void Panel_Init(void)
{
    s_mode           = MODE_MEASURE;
    s_selectedAction = ACTION_NONE;
    s_newActionFlag  = 0;
    s_debugCount     = 0;
    s_hasFreqData    = 0;

    /* 切换到横向模式：ILI9341_GramScan(3) -> X=320, Y=240 */
    ILI9341_GramScan(3);

    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

    Panel_DrawStatusBar();
    Panel_DrawButtonArea();
}

void Panel_SetMode(PanelMode_t mode)
{
    if (s_mode == mode) return;

    s_mode           = mode;
    s_selectedAction = ACTION_NONE;
    s_newActionFlag  = 0;

    Panel_DrawStatusBar();
    Panel_DrawButtonArea();

    /* 切换模式时清除右侧区域 */
    Panel_ClearRightArea();

    /* 如果切换到测量模式且有缓存的幅频数据，重新绘制曲线 */
    if (s_mode == MODE_MEASURE && s_hasFreqData)
    {
        Panel_DrawFreqResp(s_lastFreqResp, FREQRESP_POINT_NUM,
                           s_lastMidGain, s_lastCutoff);
    }
}

PanelMode_t Panel_GetMode(void)
{
    return s_mode;
}

void Panel_HandleTouch(uint16_t usX, uint16_t usY)
{
    uint8_t i, count;
    const PanelButton_t *btns;

    if (s_mode == MODE_MEASURE)
    {
        btns  = s_btnMeasure;
        count = MEASURE_BTN_COUNT;
    }
    else
    {
        btns  = s_btnFault;
        count = FAULT_BTN_COUNT;
    }

    for (i = 0; i < count; i++)
    {
        if (Panel_HitTest(&btns[i], usX, usY))
        {
            s_selectedAction = btns[i].action;
            s_newActionFlag  = 1;
            Panel_DrawButtonArea();  /* 高亮选中按钮 */
            return;
        }
    }
}

uint8_t Panel_IsNewAction(void)
{
    return s_newActionFlag;
}

PanelAction_t Panel_FetchAction(void)
{
    s_newActionFlag = 0;
    return s_selectedAction;
}

void Panel_ShowResult(const char * const *lines, uint8_t line_count)
{
    uint8_t i;

    if (line_count > PANEL_RESULT_MAX_LINES)
        line_count = PANEL_RESULT_MAX_LINES;

    /* 清除结果区（右侧上半部分：结果文字 + debug 区上方） */
    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(RIGHT_AREA_X, RIGHT_AREA_Y, RIGHT_AREA_W, 118);

    LCD_SetTextColor(PANEL_RESULT_TEXT_COLOR);
    LCD_SetBackColor(PANEL_BG_COLOR);

    for (i = 0; i < line_count; i++)
    {
        if (lines[i] != NULL)
        {
            ILI9341_DispString_EN(RIGHT_AREA_X + 4,
                                  RIGHT_AREA_Y + 4 + i * RESULT_LINE_H,
                                  (char *)lines[i]);
        }
    }
}

/* ======================== Debug 面板 ======================== */

void Panel_Debug_Clear(void)
{
    s_debugCount = 0;
    /* 不清除整个右侧，只清除 debug 区（结果区下方） */
    LCD_SetBackColor(PANEL_BG_COLOR);
    ILI9341_Clear(RIGHT_AREA_X, RIGHT_AREA_Y + 90, RIGHT_AREA_W, 30);
}

void Panel_Debug_ShowFloat(const char *label, float value, const char *unit)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%s:%.2f%s", label, (double)value, unit);

    if (s_debugCount < DEBUG_MAX_LINES)
    {
        strncpy(s_debugLines[s_debugCount], buf, sizeof(s_debugLines[0]) - 1);
        s_debugLines[s_debugCount][sizeof(s_debugLines[0]) - 1] = '\0';
        s_debugCount++;
    }

    /* 立即刷新 debug 显示区 */
    LCD_SetBackColor(PANEL_BG_COLOR);
    LCD_SetTextColor(PANEL_DEBUG_TEXT_COLOR);
    ILI9341_DispString_EN(RIGHT_AREA_X + 4,
                          RIGHT_AREA_Y + 90 + (s_debugCount - 1) * DEBUG_LINE_H,
                          buf);
}

void Panel_Debug_ShowInt(const char *label, int32_t value, const char *unit)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%s:%ld%s", label, (long)value, unit);

    if (s_debugCount < DEBUG_MAX_LINES)
    {
        strncpy(s_debugLines[s_debugCount], buf, sizeof(s_debugLines[0]) - 1);
        s_debugLines[s_debugCount][sizeof(s_debugLines[0]) - 1] = '\0';
        s_debugCount++;
    }

    LCD_SetBackColor(PANEL_BG_COLOR);
    LCD_SetTextColor(PANEL_DEBUG_TEXT_COLOR);
    ILI9341_DispString_EN(RIGHT_AREA_X + 4,
                          RIGHT_AREA_Y + 90 + (s_debugCount - 1) * DEBUG_LINE_H,
                          buf);
}

/*********************end of file*************************/
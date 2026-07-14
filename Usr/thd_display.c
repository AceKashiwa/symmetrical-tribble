#include "thd_display.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
   布局参数 —— 根据你的屏幕分辨率/方向自行微调
   ============================================================================
 */
#define WAVE_X 10
#define WAVE_Y 4
#define WAVE_W (LCD_X_LENGTH - 110) /* 320屏下 = 210px */
#define WAVE_H 70

#define SPEC_X WAVE_X
#define SPEC_Y (WAVE_Y + WAVE_H + 6) /* = 80 */
#define SPEC_W WAVE_W
#define SPEC_H 60
#define SPEC_NUM_BINS                                                          \
  32 /* 显示前32个bin（含直流），覆盖到32次谐波，足够THD分析用 */
#define SPEC_DB_FLOOR_X100 (-6000) /* -60.00dB，乘以100表示，避免浮点 */

#define INFO_X 10
#define INFO_Y (SPEC_Y + SPEC_H + 6) /* = 146 */
#define INFO_LINE_H 16 /* 5行文字，146+5*16=226，在240高度内留有余量 */

#define BTN_X (LCD_X_LENGTH - 90)
#define BTN_W 80
#define BTN_H 34
#define BTN_GAP 6
#define BTN_Y0 4

#define COLOR_BG 0xFFFF      /* 白 */
#define COLOR_AXIS 0x2104    /* 深灰，边框/坐标线 */
#define COLOR_WAVE 0xF800    /* 红，波形线 */
#define COLOR_TEXT 0x0000    /* 黑 */
#define COLOR_BTN_OFF 0xC618 /* 浅灰，按钮未选中 */
#define COLOR_BTN_ON 0x07E0  /* 绿，按钮选中 */
#define COLOR_BTN_TEXT 0x0000

#define COLOR_SPEC_BAR 0xF800  /* 红，普通频谱柱 */
#define COLOR_SPEC_FUND 0x001F /* 蓝，突出显示基波(bin 1) */

typedef struct {
  uint16_t start_x, start_y, end_x, end_y;
  const char *label;
  THD_WaveformType value;
} THD_Btn;

static THD_Btn s_buttons[5];
typedef struct {
  uint8_t pb12, pb13, pb15;
} THD_PinCombo;

/* 索引对应 THD_WaveformType，数值来自实测数据 */
static const THD_PinCombo s_waveform_pins[5] = {
    /* THD_WAVE_SINE          */ {1, 0, 1},
    /* THD_WAVE_CROSSOVER     */ {1, 0, 0},
    /* THD_WAVE_TOP_CLIP      */ {0, 1, 1},
    /* THD_WAVE_BOTTOM_CLIP   */ {1, 1, 1},
    /* THD_WAVE_BIDIRECTIONAL */ {0, 0, 1},
};

static THD_WaveformType s_current_waveform =
    THD_WAVE_SINE; /* 默认开机显示正弦波(无失真)状态 */

static void GPIO_ApplyWaveform(THD_WaveformType type) {
  const THD_PinCombo *p = &s_waveform_pins[type];

  if (p->pb12) {
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
  } else {
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
  }
  if (p->pb13) {
    GPIO_SetBits(GPIOB, GPIO_Pin_13);
  } else {
    GPIO_ResetBits(GPIOB, GPIO_Pin_13);
  }
  if (p->pb15) {
    GPIO_SetBits(GPIOB, GPIO_Pin_15);
  } else {
    GPIO_ResetBits(GPIOB, GPIO_Pin_15);
  }
}

/* 注意：本函数不触碰 PB14（用户已确认暂时闲置），如果你的项目里
   IO_Configuration() 已经把 PB14 配置成了输出，那不受影响；如果两边
   都要初始化 PB12/13/15，重复调用 GPIO_Init 是无害的，只是多做一次。 */
static void THD_GPIO_Init(void) {
  GPIO_InitTypeDef gpio;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  gpio.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
  gpio.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &gpio);

  GPIO_ApplyWaveform(s_current_waveform);
}

/* ============================================================================
   按钮布局与绘制
   ============================================================================
 */
static void Layout_Buttons(void) {
  static const char *labels[5] = {"SINE", "X-OVER", "TOP CLIP", "BOT CLIP",
                                  "BI-DIR"};
  int i;

  for (i = 0; i < 5; i++) {
    s_buttons[i].start_x = BTN_X;
    s_buttons[i].start_y = BTN_Y0 + i * (BTN_H + BTN_GAP);
    s_buttons[i].end_x = BTN_X + BTN_W;
    s_buttons[i].end_y = s_buttons[i].start_y + BTN_H;
    s_buttons[i].label = labels[i];
    s_buttons[i].value = (THD_WaveformType)i; /* 数组下标直接对应枚举顺序 */
  }
}

static void Draw_Button(uint8_t index) {
  THD_Btn *b = &s_buttons[index];
  uint8_t is_active;
  uint16_t text_w, text_x, text_y;

  is_active = (s_current_waveform == b->value);

  LCD_SetColors(is_active ? COLOR_BTN_ON : COLOR_BTN_OFF, COLOR_BG);
  ILI9341_DrawRectangle(b->start_x, b->start_y, b->end_x - b->start_x,
                        b->end_y - b->start_y, 1);

  LCD_SetColors(COLOR_AXIS, COLOR_BG);
  ILI9341_DrawRectangle(b->start_x, b->start_y, b->end_x - b->start_x,
                        b->end_y - b->start_y, 0);

  LCD_SetFont(&Font8x16);
  LCD_SetColors(COLOR_BTN_TEXT, is_active ? COLOR_BTN_ON : COLOR_BTN_OFF);

  text_w = (uint16_t)strlen(b->label) * 8U; /* Font8x16 每字符约8px宽 */
  text_x = b->start_x + (((b->end_x - b->start_x) > text_w)
                             ? ((b->end_x - b->start_x) - text_w) / 2
                             : 2);
  text_y = b->start_y + ((b->end_y - b->start_y) - 16) / 2;

  ILI9341_DispString_EN(text_x, text_y, (char *)b->label);
}

static void Draw_All_Buttons(void) {
  int i;
  for (i = 0; i < 5; i++) {
    Draw_Button((uint8_t)i);
  }
}

/* ============================================================================
   波形绘制
   ============================================================================
 */
static void Find_MinMax(uint16_t *out_min, uint16_t *out_max) {
  uint16_t vmin = ADC_AverageValue[0];
  uint16_t vmax = ADC_AverageValue[0];
  int i;

  for (i = 1; i < THD_DISPLAY_NUM_SAMPLES; i++) {
    uint16_t v = ADC_AverageValue[i];
    if (v < vmin) {
      vmin = v;
    }
    if (v > vmax) {
      vmax = v;
    }
  }
  *out_min = vmin;
  *out_max = vmax;
}

void THD_Display_UpdateWaveform(void) {
  uint16_t vmin, vmax, span;
  uint16_t prev_x, prev_y;
  int i;

  Find_MinMax(&vmin, &vmax);
  span = (vmax > vmin) ? (uint16_t)(vmax - vmin) : 1U; /* 避免平坦信号除零 */

  /* 清空波形区域 */
  LCD_SetColors(COLOR_BG, COLOR_BG);
  ILI9341_DrawRectangle(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, 1);

  /* 边框 */
  LCD_SetColors(COLOR_AXIS, COLOR_BG);
  ILI9341_DrawRectangle(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, 0);

  /* 波形折线 */
  LCD_SetColors(COLOR_WAVE, COLOR_BG);

  prev_x = WAVE_X;
  prev_y =
      (uint16_t)(WAVE_Y + WAVE_H - 1 -
                 (uint32_t)(ADC_AverageValue[0] - vmin) * (WAVE_H - 1) / span);

  for (i = 1; i < THD_DISPLAY_NUM_SAMPLES; i++) {
    uint16_t x = (uint16_t)(WAVE_X + (uint32_t)i * (WAVE_W - 1) /
                                         (THD_DISPLAY_NUM_SAMPLES - 1));
    uint16_t y = (uint16_t)(WAVE_Y + WAVE_H - 1 -
                            (uint32_t)(ADC_AverageValue[i] - vmin) *
                                (WAVE_H - 1) / span);

    ILI9341_DrawLine(prev_x, prev_y, x, y);

    prev_x = x;
    prev_y = y;
  }
}

/**
 * @brief 近似计算 log2(x)，返回值放大256倍（Q8定点）。
 *        用 CLZ 指令求最高有效位得到整数部分，小数部分用线性插值近似，
 *        在 log2 值域上最大误差约 ±0.09（换算到dB约 ±0.5dB）。
 *        仅用于柱状图可视化，不用于精确测量。x 必须 > 0。
 */
static int32_t approx_log2_q8(uint32_t x) {
  int msb =
      31 - __builtin_clz(x); /* floor(log2(x))，硬件CLZ指令，单周期级开销 */
  uint32_t mantissa;

  if (msb >= 16) {
    mantissa = x >> (msb - 16);
  } else {
    mantissa = x << (16 - msb);
  }
  /* mantissa 落在 [65536, 131071]，即 x/2^msb 在 Q16 定点下的 [1.0, 2.0) */

  uint32_t frac_q16 = mantissa - 65536U; /* 小数部分 f，Q16，范围 0..65535 */

  return (int32_t)(msb << 8) +
         (int32_t)(frac_q16 >> 8); /* 整数.小数 部分，Q8 */
}

void THD_Display_UpdateSpectrum(void) {
  uint32_t max_mag = 1; /* 至少为1，避免除零 */
  int32_t log2_max_q8;
  int i;
  uint16_t bar_w;

  for (i = 0; i < SPEC_NUM_BINS; i++) {
    if (FFT_Magnitude[i] > max_mag) {
      max_mag = FFT_Magnitude[i];
    }
  }
  log2_max_q8 = approx_log2_q8(max_mag);

  /* 清空频谱区域 */
  LCD_SetColors(COLOR_BG, COLOR_BG);
  ILI9341_DrawRectangle(SPEC_X, SPEC_Y, SPEC_W, SPEC_H, 1);

  /* 边框 */
  LCD_SetColors(COLOR_AXIS, COLOR_BG);
  ILI9341_DrawRectangle(SPEC_X, SPEC_Y, SPEC_W, SPEC_H, 0);

  bar_w = (uint16_t)((SPEC_W - 2) / SPEC_NUM_BINS);
  if (bar_w < 1) {
    bar_w = 1;
  }

  for (i = 0; i < SPEC_NUM_BINS; i++) {
    int32_t bar_h;
    uint16_t bar_x, bar_y, draw_w;

    if (FFT_Magnitude[i] == 0) {
      bar_h = 0;
    } else {
      int32_t diff_q8 = approx_log2_q8(FFT_Magnitude[i]) -
                        log2_max_q8;           /* log2(ratio), Q8, <=0 */
      int32_t db_x100 = (602 * diff_q8) / 256; /* ~= 20*log10(ratio)，乘以100 */

      if (db_x100 < SPEC_DB_FLOOR_X100) {
        db_x100 = SPEC_DB_FLOOR_X100;
      }

      bar_h =
          (db_x100 - SPEC_DB_FLOOR_X100) * (SPEC_H - 2) / (-SPEC_DB_FLOOR_X100);
      if (bar_h < 0) {
        bar_h = 0;
      }
    }

    bar_x = (uint16_t)(SPEC_X + 1 + i * bar_w);
    bar_y = (uint16_t)(SPEC_Y + SPEC_H - 1 - bar_h);
    draw_w = (bar_w > 1) ? (bar_w - 1) : 1;

    LCD_SetColors((i == 1) ? COLOR_SPEC_FUND : COLOR_SPEC_BAR, COLOR_BG);
    ILI9341_DrawRectangle(bar_x, bar_y, draw_w,
                          (bar_h > 0) ? (uint16_t)bar_h : 1, 1);
  }
}

/* ============================================================================
   数值面板（THD% + 关键电压值）
   ============================================================================
 */
static uint32_t Code_To_Millivolts(uint16_t code) {
  return (uint32_t)code * THD_DISPLAY_VREF_MV / THD_DISPLAY_ADC_MAX;
}

/* 手动格式化毫伏值为 "X.XXV" 字符串，不依赖 printf 的 %f 浮点格式化器
   (避免之前踩过的、和工具链 multilib/interworking 相关的链接问题) */
static void Format_Millivolts(char *buf, size_t buf_size, uint32_t mv) {
  snprintf(buf, buf_size, "%lu.%02luV", (unsigned long)(mv / 1000),
           (unsigned long)((mv % 1000) / 10));
}

void THD_Display_UpdateValues(void) {
  char line[40];
  char vmax_str[12], vmin_str[12], vpp_str[12], vavg_str[12];
  uint16_t vmin_code, vmax_code, vavg_code;
  uint32_t vavg_sum = 0;
  uint32_t vmax_mv, vmin_mv, vpp_mv, vavg_mv;
  uint32_t thd_x100;
  int i;

  Find_MinMax(&vmin_code, &vmax_code);
  for (i = 0; i < THD_DISPLAY_NUM_SAMPLES; i++) {
    vavg_sum += ADC_AverageValue[i];
  }
  vavg_code = (uint16_t)(vavg_sum / THD_DISPLAY_NUM_SAMPLES);

  vmax_mv = Code_To_Millivolts(vmax_code);
  vmin_mv = Code_To_Millivolts(vmin_code);
  vpp_mv = vmax_mv - vmin_mv;
  vavg_mv = Code_To_Millivolts(vavg_code);

  /* THD 定点百分比 (x100)，例如 327 表示 "3.27%"，同样不依赖 %f */
  thd_x100 = (uint32_t)(THD * 10000.0f + 0.5f);

  LCD_SetFont(&Font8x16);

  /* 先清空数值面板区域（5行文字高度） */
  LCD_SetColors(COLOR_BG, COLOR_BG);
  ILI9341_DrawRectangle(INFO_X, INFO_Y, WAVE_W, INFO_LINE_H * 5, 1);

  LCD_SetColors(COLOR_TEXT, COLOR_BG);

  snprintf(line, sizeof(line), "THD:  %lu.%02lu %%",
           (unsigned long)(thd_x100 / 100), (unsigned long)(thd_x100 % 100));
  ILI9341_DispString_EN(INFO_X, INFO_Y + 0 * INFO_LINE_H, line);

  Format_Millivolts(vmax_str, sizeof(vmax_str), vmax_mv);
  Format_Millivolts(vmin_str, sizeof(vmin_str), vmin_mv);
  Format_Millivolts(vpp_str, sizeof(vpp_str), vpp_mv);
  Format_Millivolts(vavg_str, sizeof(vavg_str), vavg_mv);

  snprintf(line, sizeof(line), "Vmax: %s", vmax_str);
  ILI9341_DispString_EN(INFO_X, INFO_Y + 1 * INFO_LINE_H, line);

  snprintf(line, sizeof(line), "Vmin: %s", vmin_str);
  ILI9341_DispString_EN(INFO_X, INFO_Y + 2 * INFO_LINE_H, line);

  snprintf(line, sizeof(line), "Vpp:  %s", vpp_str);
  ILI9341_DispString_EN(INFO_X, INFO_Y + 3 * INFO_LINE_H, line);

  snprintf(line, sizeof(line), "Vavg: %s", vavg_str);
  ILI9341_DispString_EN(INFO_X, INFO_Y + 4 * INFO_LINE_H, line);
}

/* ============================================================================
   触摸事件处理
   ============================================================================
 */
void THD_Display_TouchDown(uint16_t x, uint16_t y) {
  int i;

  for (i = 0; i < 5; i++) {
    THD_Btn *b = &s_buttons[i];

    if (x >= b->start_x && x <= b->end_x && y >= b->start_y && y <= b->end_y) {
      if (b->value != s_current_waveform) {
        s_current_waveform = b->value;
        GPIO_ApplyWaveform(s_current_waveform);
        Draw_All_Buttons(); /* 重绘全部5个按钮，取消旧选项高亮 */
      }
      return;
    }
  }
}

void THD_Display_TouchUp(uint16_t x, uint16_t y) {
  (void)x;
  (void)y;
  /* 选择已在 TouchDown 时立即生效，这里不需要额外动作 */
}

/* ============================================================================
   初始化
   ============================================================================
 */
void THD_Display_Init(void) {
  THD_GPIO_Init();
  Layout_Buttons();

  LCD_SetBackColor(COLOR_BG);
  ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

  Draw_All_Buttons();
  THD_Display_UpdateWaveform();
  THD_Display_UpdateValues();
  THD_Display_UpdateValues();
}

THD_WaveformType THD_Display_GetWaveformType(void) {
  return s_current_waveform;
}
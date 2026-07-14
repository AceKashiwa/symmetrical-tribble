# THD Toolkit

STM32F103 total-harmonic-distortion (THD) measurement project的上位机（host-side）Python 工具集，
统一为一个 CLI，包含三个子命令：

| 子命令      | 功能                                              |
|-------------|---------------------------------------------------|
| `twiddle`   | 生成/更新 `rfft_q15_128.c` 用的 128 点 FFT 旋转因子表 |
| `hanning`   | 生成 Q15 定点 Hanning 窗表                         |
| `waveform`  | 通过串口或离线粘贴/文件，采集并绘制 ADC 波形        |

## 安装

```bash
uv sync                  # 基础依赖 (matplotlib, numpy)
uv sync --extra serial   # 需要串口实时采集时，额外装 pyserial
```

## 用法

```bash
# 生成旋转因子表，打印到 stdout
uv run thd-toolkit twiddle

# 就地更新固件源码里的表
uv run thd-toolkit twiddle --update path/to/rfft_q15_128.c

# 生成 Hanning 窗表
uv run thd-toolkit hanning

# 串口实时采集一帧并画图
uv run thd-toolkit waveform --port COM5 --output waveform.png

# 离线模式：粘贴/读取已保存的串口日志
uv run thd-toolkit waveform --offline --input-file capture.txt
```

## 项目背景

配套的 STM32F103 固件通过 DMA 以约 128 kHz 采集 ADC，做相干平均后送入 128 点定点 radix-2 FFT，
计算 THD。本仓库是给这条链路配套的开发期工具（生成查找表、离线核对波形），不在设备上运行。
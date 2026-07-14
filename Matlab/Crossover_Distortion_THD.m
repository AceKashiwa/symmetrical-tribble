function Crossover_Distortion_THD()
    clc; clear; close all;

    %% 1. 参数设置
    fs = 48000;           % 采样率 (Hz)
    f0 = 1000;            % 基波频率 (Hz)
    duration = 0.01;      % 信号时长 (秒)
    V_input = 1;          % 输入信号幅度
    V_bias = 0.4;         % 交越失真阈值（模拟晶体管开启电压，单位：归一化幅度）
    
    % 时间向量
    t = 0:1/fs:duration-1/fs;
    num_samples = length(t);

    %% 2. 生成输入信号与模拟交越失真
    % 正弦波输入
    Vin = V_input * sin(2*pi*f0*t);
    
    % --- 核心：交越失真模型 ---
    % 原理：当 |Vin| < V_bias 时，输出为 0 (死区)
    %       当 |Vin| >= V_bias 时，输出跟随输入
    % 使用 sign() 和 max() 函数构建该非线性特性
    Vout = sign(Vin) .* max(0, abs(Vin) - V_bias);

    %% 3. 计算总谐波失真 (THD)
    % 去除直流分量
    Vout_ac = Vout - mean(Vout);
    
    % 执行 FFT
    NFFT = 2^nextpow2(num_samples);
    Y = fft(Vout_ac, NFFT)/num_samples;
    f = fs/2 * linspace(0,1,NFFT/2+1);
    Y = Y(1:NFFT/2+1);
    
    % 寻找基波频率 bin (f0)
    [~, idx_f0] = min(abs(f - f0));
    
    % 提取基波幅度 (RMS值)
    % FFT 单边谱幅度转换为 RMS: 幅值 / sqrt(2)
    A_f0_rms = abs(Y(idx_f0)) * sqrt(2);
    
    % 计算谐波能量 (RMS)
    % 通常计算前 5~10 次谐波，或者直到 fs/2
    harmonic_indices = setdiff(round(idx_f0*(2:10)'), idx_f0); % 取 2~10 次谐波
    harmonic_indices = harmonic_indices(harmonic_indices <= length(Y)); % 防止越界
    
    harmonic_sum_rms = 0;
    for k = harmonic_indices'
        if k <= length(Y)
            harmonic_sum_rms = harmonic_sum_rms + abs(Y(k))^2;
        end
    end
    harmonic_sum_rms = sqrt(harmonic_sum_rms); % 总谐波 RMS
    
    % 计算 THD (%)
    THD_percent = (harmonic_sum_rms / A_f0_rms) * 100;
    
    fprintf('总谐波失真 (THD): %.4f%%\n', THD_percent);

    %% 4. 绘图
    figure('Color', 'w', 'Position', [100, 100, 1000, 600]);
    
    % 子图 1: 时域波形
    subplot(2, 1, 1);
    plot(t*1000, Vin, 'LineWidth', 1.5, 'DisplayName', '输入信号');
    hold on;
    plot(t*1000, Vout, 'LineWidth', 1.5, 'DisplayName', '输出(含失真)');
    xlabel('时间 (ms)'); ylabel('幅度');
    title(sprintf('交越失真模拟 (阈值 = %.1fV)', V_bias));
    legend; grid on;
    axis([0 5 -1.2 1.2]); % 局部放大观察交越点
    
    % 子图 2: 频谱分析
    subplot(2, 1, 2);
    stem(f/1000, abs(Y)*2, 'filled', 'MarkerSize', 3);
    xlabel('频率 (kHz)'); ylabel('幅度');
    title('频谱分析 (FFT)');
    grid on;
    xlim([0 10]); % 观察 0-10kHz 范围
end
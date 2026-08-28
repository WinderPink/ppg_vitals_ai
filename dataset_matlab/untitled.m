%% Script: Extract_PPG_and_BP_to_CSV.m
% Mục đích: Load file .mat, trích xuất tín hiệu PPG (Red, IR) và ghép chung
% với dữ liệu huyết áp (SBP, DBP) để xuất ra CSV.

clc; clear; close all;

%% 1. Tự động load file .mat (Xử lý linh hoạt tên biến)
clear input_data output_data;

% Load dữ liệu từ 2 file .mat vào 2 biến tạm dạng struct
temp_input = load('input_data.mat');
temp_output = load('output_data.mat');

% Lấy tên biến thực tế bên trong file .mat
name_input = fieldnames(temp_input);
name_output = fieldnames(temp_output);

% Gán dữ liệu đó vào biến input_data và output_data
input_data = temp_input.(name_input{1});
output_data = temp_output.(name_output{1});

% Kiểm tra kích thước dữ liệu
disp(['Kích thước input_data: ', num2str(size(input_data))]);  % Ví dụ: 127x1 cell
disp(['Kích thước output_data: ', num2str(size(output_data))]); % Ví dụ: 127x2 double

%% 2. Khởi tạo tham số và biến
WINDOW_SIZE = 800;   % Số mẫu PPG lấy từ mỗi bản ghi. Nếu muốn lấy toàn bộ 1000, sửa thành 1000.
num_records = length(input_data);

% Khởi tạo ma trận chứa dữ liệu PPG
data_red = zeros(num_records, WINDOW_SIZE); 
data_ir  = zeros(num_records, WINDOW_SIZE);

% Lấy dữ liệu huyết áp từ output_data (đã bỏ hoàn toàn tính HR, SpO2)
SBP = output_data(:, 1);
DBP = output_data(:, 2);

%% 3. Duyệt từng bản ghi để trích xuất Red và IR
for i = 1:num_records
    sig = input_data{i};   % Dạng 1000x2: cột1=IR, cột2=Red
    ir_raw = sig(:, 1);
    red_raw = sig(:, 2);
    
    % Cắt lấy WINDOW_SIZE mẫu đầu tiên
    red = red_raw(1:WINDOW_SIZE);
    ir  = ir_raw(1:WINDOW_SIZE);
    
    data_red(i, :) = red';
    data_ir(i, :)  = ir';
end

%% 4. Xuất file CSV (Chỉ gộp Input + Output)
% Tạo tên cột cho dữ liệu PPG
red_cols = cell(1, WINDOW_SIZE);
ir_cols  = cell(1, WINDOW_SIZE);
for j = 1:WINDOW_SIZE
    red_cols{j} = sprintf('red_z_%d', j);
    ir_cols{j}  = sprintf('ir_z_%d', j);
end

% Ghép tên cột: Dữ liệu Red + Dữ liệu IR + SBP + DBP
all_cols = [red_cols, ir_cols, {'SBP'}, {'DBP'}];

% Ghép dữ liệu thành bảng
T = array2table([data_red, data_ir, SBP, DBP], ...
    'VariableNames', all_cols);

% Xuất file CSV
output_file = 'ppg_sequence_data_1.csv';
writetable(T, output_file);
fprintf('Đã xuất file CSV: %s\n', output_file);
fprintf('Tổng số mẫu (số hàng): %d\n', num_records);
fprintf('Tổng số cột (dữ liệu PPG + 2 cột huyết áp): %d\n', WINDOW_SIZE * 2 + 2);
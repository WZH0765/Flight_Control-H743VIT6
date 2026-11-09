#pragma once

#define ALT_CENTER    500.0f    //杆量中位（自动回中，定高）
#define ALT_DEADBAND  20.0f     //中位附近这点死区不算，免得抖
#define ALT_VZ_MAX    60.0f     //满偏升降速度（cm/s）
#define ALT_CLIMB     990.0f    //满杆判定（‰）：到这儿油门直接顶满，不再走高度环
#define ALT_MAX_UP    500.0f    //目标高度上限（cm）
#define ALT_IDLE      50.0f     //落地的电机怠速（‰）
#define ALT_HOVER     421.0f    //悬停基准油门（‰），真机要按实际重量标定
#define ALT_FF        0.22f     //升降速度前馈（‰ per cm/s）
#define ALT_KP        0.60f     //‰/cm（别调太高，高了油门会来回振荡）
#define ALT_KD        1.00f     //‰/(cm/s)，阻尼用，符号必须是"上升减油门"
#define ALT_ERR_MAX   200.0f    //误差参与运算上限（cm），防止冲过头把油门压到 0
#define ALT_RATE_MAX  200.0f    //爬升率参与运算上限（cm/s）
#define ALT_SLEW      4000.0f   //油门变化率上限（‰/s）
#define ALT_LP_ALT    0.15f     //高度低通（s），气压计噪声大
#define ALT_LP_RATE   0.05f     //升降速度低通（s）

float Height_Update(float StickThro,float dt);   //返回总油门（‰）
#include "AHRS.h"
#include <math.h>

euler_t AHRS = {0};

static float Gx_Bias = 0,Gy_Bias = 0,Gz_Bias = 0;
static float q0 = 1.0f,q1 = 0.0f,q2 = 0.0f,q3 = 0.0f;

void AHRS_Update(float Ax,float Ay,float Az,float Gx,float Gy,float Gz,float Mx,float My,float Mz,float dt)
{
    float An = sqrtf(Ax*Ax + Ay*Ay + Az*Az);
    if(An > 0.01f)
    {
        Ax /= An; Ay /= An; Az /= An;
        float A_vx = 2.0f*(q1*q3 - q0*q2);
        float A_vy = 2.0f*(q0*q1 + q2*q3);
        float A_vz = q0*q0 - q1*q1 - q2*q2 + q3*q3;

        float A_ex = Ay*A_vz - Az*A_vy;
        float A_ey = Az*A_vx - Ax*A_vz;
        float A_ez = Ax*A_vy - Ay*A_vx;

        Gx_Bias += KI_ACC*A_ex*dt;
        Gy_Bias += KI_ACC*A_ey*dt;
        Gz_Bias += KI_ACC*A_ez*dt;

        Gx += KP_ACC*A_ex;
        Gy += KP_ACC*A_ey;
        Gz += KP_ACC*A_ez;
    }
    // float Mn = sqrtf(Mx*Mx + My*My + Mz*Mz);
    // if(Mn > 0.01f)
    // {
    //     Mx /= Mn; My /= Mn; Mz /= Mn;
    //     float hx = 2.0f*(Mx*(0.5f - q2*q2 - q3*q3)+My*(q1*q2 - q0*q3)+Mz*(q1*q3 + q0*q2));
    //     float hy = 2.0f*(Mx*(q1*q2 + q0*q3)+My*(0.5f - q1*q1 - q3*q3)+Mz*(q2*q3 - q0*q1));
        
    //     float bx = sqrtf(hx*hx + hy*hy);
    //     float bz = 2.0f*(Mx*(q1*q3 - q0*q2)+My*(q2*q3 + q0*q1)+Mz*(0.5f - q1*q1 - q2*q2));

    //     float M_vx = 2.0f * (bx*(0.5f - q2*q2 - q3*q3) + bz*(q1*q3 - q0*q2));
    //     float M_vy = 2.0f * (bx*(q1*q2 - q0*q3) + bz*(q2*q3 + q0*q1));
    //     float M_vz = 2.0f * (bx*(q1*q3 + q0*q2) + bz*(0.5f - q1*q1 - q2*q2));

    //     float M_ex = My*M_vz - Mz*M_vy;
    //     float M_ey = Mz*M_vx - Mx*M_vz;
    //     float M_ez = Mx*M_vy - My*M_vx;

    //     Gx_Bias += KI_MAG*M_ex*dt;
    //     Gy_Bias += KI_MAG*M_ey*dt;
    //     Gz_Bias += KI_MAG*M_ez*dt;

    //     Gx += KP_MAG*M_ex;
    //     Gy += KP_MAG*M_ey;
    //     Gz += KP_MAG*M_ez;
    // }

    Gx += Gx_Bias;
    Gy += Gy_Bias;
    Gz += Gz_Bias;

    float q0d = 0.5f*(-q1*Gx - q2*Gy - q3*Gz);
    float q1d = 0.5f*( q0*Gx + q2*Gz - q3*Gy);
    float q2d = 0.5f*( q0*Gy - q1*Gz + q3*Gx);
    float q3d = 0.5f*( q0*Gz + q1*Gy - q2*Gx);

    q0 += q0d*dt;
    q1 += q1d*dt;
    q2 += q2d*dt;
    q3 += q3d*dt;

    float n = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 /= n; q1 /= n; q2 /= n; q3 /= n;

    AHRS.Yaw   = atan2f(2.0f*(q0*q3 + q1*q2),1.0f - 2.0f*(q2*q2 + q3*q3));
    AHRS.Roll  = atan2f(2.0f*(q0*q1 + q2*q3),1.0f - 2.0f*(q1*q1 + q2*q2));
    AHRS.Pitch = asinf (2.0f*(q0*q2 - q3*q1));
}

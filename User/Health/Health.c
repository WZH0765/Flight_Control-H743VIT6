#include "Health.h"

void Health_Init(health_t *H,uint16_t Limit)
{
    H->Last[0] = 0;
    H->Last[1] = 0;
    H->Last[2] = 0;

    H->Count   = 0;
    H->Limit   = Limit;
}

void Health_Update(health_t *H,int32_t V0,int32_t V1,int32_t V2)
{
    if(V0 == H->Last[0] && V1 == H->Last[1] && V2 == H->Last[2])
    {
        if(H->Count < 0xFFFF) H->Count ++;
    }
    else
    {
        H->Count   = 0;
        H->Last[0] = V0;
        H->Last[1] = V1;
        H->Last[2] = V2;
    }
}

bool Health_Status(health_t *H)
{
    return H->Count < H->Limit;
}

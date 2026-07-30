/*
 * IoHwAb.c - I/O Hardware Abstraction 구현
 * 포트 핀/ADC 채널 매핑이 하드웨어 개정으로 바뀌면 "여기만" 고친다.
 */
#include "IoHwAb.h"
#include "Dio.h"
#include "Adc.h"

void IoHwAb_Init(void) { }

Std_ReturnType IoHwAb_BatteryVoltage_Get(uint16* raw12)
{
    if (raw12 == NULL) { return E_NOT_OK; }
    return Adc_ReadGroup(ADC_CH_BATTERY, (Adc_ValueGroupType*)raw12);
}

Std_ReturnType IoHwAb_AmbientLight_Get(uint16* raw12)
{
    if (raw12 == NULL) { return E_NOT_OK; }
    return Adc_ReadGroup(ADC_CH_AMBIENT, (Adc_ValueGroupType*)raw12);
}

uint8 IoHwAb_LightSwitch_Get(void)
{
    return (uint8)Dio_ReadChannel(DIO_CH_LIGHT_SWITCH);
}

void IoHwAb_Headlight_Set(uint8 level)
{
    Dio_WriteChannel(DIO_CH_HEADLIGHT, (level != 0u) ? STD_HIGH : STD_LOW);
}

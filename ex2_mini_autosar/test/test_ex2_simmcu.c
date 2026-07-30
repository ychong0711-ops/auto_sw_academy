/*
 * [EX2 SimMcu 단위 테스트] 가상 MCU GPIO/ADC 기능 검증
 *
 * SimMcu 는 모든 MCAL 의존성이 집중되는 계층이므로,
 * GPIO(InjectPin/GetOutputPin)와 ADC(InjectAdc/ReadAdcRaw)의
 * 정합성을 독립 검증한다.
 *
 * 요구사항 추적: [SRS_SIM_010] GPIO 핀 주입/관찰 정확성
 *               [SRS_SIM_020] ADC 값 주입/읽기 정확성
 */
#include "mini_test.h"
#include "SimMcu.h"

int main(void)
{
    SimMcu_Init();

    MT_SECTION("TC_SIM_010: Init 후 모든 레지스터 = 0");
    {
        MT_CHECK(SimMcu_GpioOdr == 0u, "Init: ODR = 0");
        MT_CHECK(SimMcu_GpioIdr == 0u, "Init: IDR = 0");
        for (uint8 ch = 0u; ch < SIMMCU_ADC_CH_MAX; ch++) {
            MT_CHECK(SimMcu_ReadAdcRaw(ch) == 0u, "Init: ADC ch0 = 0");
        }
    }

    MT_SECTION("TC_SIM_020: InjectPin -> IDR 레지스터 반영");
    {
        SimMcu_InjectPin(0u, 1u);
        MT_CHECK((SimMcu_GpioIdr & 0x01u) != 0u, "Pin 0 HIGH: IDR bit0 set");

        SimMcu_InjectPin(3u, 1u);
        MT_CHECK((SimMcu_GpioIdr & 0x09u) == 0x09u, "Pin 0+3 HIGH: IDR bits 0,3 set");

        SimMcu_InjectPin(0u, 0u);
        MT_CHECK((SimMcu_GpioIdr & 0x01u) == 0u, "Pin 0 LOW: IDR bit0 cleared");
        MT_CHECK((SimMcu_GpioIdr & 0x08u) != 0u, "Pin 3 still HIGH");
    }

    MT_SECTION("TC_SIM_030: InjectPin 범위 초과 무시");
    {
        uint8 before = SimMcu_GpioIdr;
        SimMcu_InjectPin(8u, 1u);  /* pin >= 8 -> return */
        MT_CHECK(SimMcu_GpioIdr == before, "Pin 8 OOR: IDR unchanged");
        SimMcu_InjectPin(255u, 1u);
        MT_CHECK(SimMcu_GpioIdr == before, "Pin 255 OOR: IDR unchanged");
    }

    MT_SECTION("TC_SIM_040: GetOutputPin -> ODR 레지스터 읽기");
    {
        /* ODR 은 원래 0이므로 모든 출력 LOW */
        for (uint8 pin = 0u; pin < 8u; pin++) {
            MT_CHECK(SimMcu_GetOutputPin(pin) == 0u, "Init: 모든 출력 LOW");
        }

        /* ODR 직접 설정 -> GetOutputPin 반영 */
        SimMcu_GpioOdr = 0xAAu;  /* 0b10101010 */
        MT_CHECK(SimMcu_GetOutputPin(0u) == 0u, "ODR=0xAA: pin0 = 0");
        MT_CHECK(SimMcu_GetOutputPin(1u) == 1u, "ODR=0xAA: pin1 = 1");
        MT_CHECK(SimMcu_GetOutputPin(7u) == 1u, "ODR=0xAA: pin7 = 1");

        SimMcu_Init();  /* 복구 */
    }

    MT_SECTION("TC_SIM_050: GetOutputPin 범위 초과 -> 0");
    {
        MT_CHECK(SimMcu_GetOutputPin(8u) == 0u, "Pin 8 OOR: 0 반환");
        MT_CHECK(SimMcu_GetOutputPin(255u) == 0u, "Pin 255 OOR: 0 반환");
    }

    MT_SECTION("TC_SIM_060: InjectAdc -> ReadAdcRaw 정합");
    {
        SimMcu_InjectAdc(0u, 0x0FFFu);
        MT_CHECK(SimMcu_ReadAdcRaw(0u) == 0x0FFFu, "ADC ch0 = 4095 (12bit max)");

        SimMcu_InjectAdc(0u, 0u);
        MT_CHECK(SimMcu_ReadAdcRaw(0u) == 0u, "ADC ch0 = 0");

        SimMcu_InjectAdc(3u, 0x0555u);
        MT_CHECK(SimMcu_ReadAdcRaw(3u) == 0x0555u, "ADC ch3 = 0x555");
    }

    MT_SECTION("TC_SIM_070: InjectAdc 12비트 마스킹");
    {
        SimMcu_InjectAdc(1u, 0xFFFFu);
        MT_CHECK(SimMcu_ReadAdcRaw(1u) == 0x0FFFu, "ADC 0xFFFF masked to 12bit = 0xFFF");

        SimMcu_InjectAdc(1u, 0x1234u);
        MT_CHECK(SimMcu_ReadAdcRaw(1u) == 0x0234u, "ADC 0x1234 masked to 12bit = 0x234");
    }

    MT_SECTION("TC_SIM_080: ADC 범위 초과 채널 -> 0");
    {
        SimMcu_InjectAdc(8u, 0x0FFFu);         /* ch >= SIMMCU_ADC_CH_MAX -> 무시 */
        MT_CHECK(SimMcu_ReadAdcRaw(8u) == 0u, "ADC ch8 OOR: Inject 무시");
        MT_CHECK(SimMcu_ReadAdcRaw(8u) == 0u, "ADC ch8 OOR: Read = 0");
    }

    MT_SECTION("TC_SIM_090: 다중 ADC 채널 독립성");
    {
        SimMcu_InjectAdc(0u, 0x0AAAu);
        SimMcu_InjectAdc(1u, 0x0BBBu);
        SimMcu_InjectAdc(2u, 0x0CCCu);
        MT_CHECK(SimMcu_ReadAdcRaw(0u) == 0x0AAAu, "ADC ch0 independent");
        MT_CHECK(SimMcu_ReadAdcRaw(1u) == 0x0BBBu, "ADC ch1 independent");
        MT_CHECK(SimMcu_ReadAdcRaw(2u) == 0x0CCCu, "ADC ch2 independent");
    }

    MT_SUMMARY();
}

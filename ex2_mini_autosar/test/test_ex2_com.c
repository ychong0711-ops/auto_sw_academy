/*
 * [EX2 Com 단위 테스트] 시그널 패킹/언패킹 (RX 경로)
 *
 * 검증: Com_RxIndication 으로 수신된 I-PDU 데이터가
 *       Com_ReceiveSignal 로 올바르게 언패킹되는지 확인.
 *       pack_le/unpack_le 비트 조작 로직을 public API 를 통해 간접 검증.
 *
 * 요구사항 추적: [SRS_COM_010] 시그널 값을 I-PDU 버퍼에 패킹
 *               [SRS_COM_020] 수신 I-PDU 로부터 시그널 언패킹
 *               [SRS_COM_040] 하위 계층 수신 통지 처리
 */
#include "mini_test.h"
#include "Com.h"

int main(void)
{
    Com_Init();

    MT_SECTION("TC_COM_010: 미수신 상태에서 ReceiveSignal -> E_NOT_OK");
    {
        uint32 val = 0xDeadBeefu;
        Std_ReturnType ret = Com_ReceiveSignal(COM_SIG_OverrideMode, &val);
        MT_CHECK(ret == E_NOT_OK, "미수신: E_NOT_OK 반환");
        MT_CHECK(val == 0xDeadBeefu, "미수신: 출력 버퍼 미변경");
    }

    MT_SECTION("TC_COM_020: LightOverride 수신 -> 시그널 언패킹");
    {
        /* LightOverride: OverrideMode=0x02(ForceOff), OverrideKey=0x5A */
        uint8 rxBuf[2] = { 0x02u, 0x5Au };
        PduInfoType pdu;
        pdu.SduDataPtr = rxBuf;
        pdu.SduLength = 2u;
        Com_RxIndication(COM_RXPDU_LightOverride, &pdu);

        uint32 mode = 99u;
        Std_ReturnType ret = Com_ReceiveSignal(COM_SIG_OverrideMode, &mode);
        MT_CHECK(ret == E_OK, "OverrideMode: E_OK");
        MT_CHECK(mode == 0x02u, "OverrideMode = 2 (ForceOff)");

        uint32 key = 99u;
        ret = Com_ReceiveSignal(COM_SIG_OverrideKey, &key);
        MT_CHECK(ret == E_OK, "OverrideKey: E_OK");
        MT_CHECK(key == 0x5Au, "OverrideKey = 0x5A");
    }

    MT_SECTION("TC_COM_030: 시그널 경계값 언패킹");
    {
        /* OverrideMode=0xFF (최대), OverrideKey=0xFF */
        uint8 rxBuf[2] = { 0xFFu, 0xFFu };
        PduInfoType pdu;
        pdu.SduDataPtr = rxBuf;
        pdu.SduLength = 2u;
        Com_RxIndication(COM_RXPDU_LightOverride, &pdu);

        uint32 mode;
        Com_ReceiveSignal(COM_SIG_OverrideMode, &mode);
        MT_CHECK(mode == 0xFFu, "OverrideMode max = 0xFF");

        uint32 key;
        Com_ReceiveSignal(COM_SIG_OverrideKey, &key);
        MT_CHECK(key == 0xFFu, "OverrideKey max = 0xFF");
    }

    MT_SECTION("TC_COM_040: 알 수 없는 SignalId -> E_NOT_OK");
    {
        uint32 val = 0u;
        Std_ReturnType ret = Com_ReceiveSignal(99u, &val);
        MT_CHECK(ret == E_NOT_OK, "존재하지 않는 SignalId: E_NOT_OK");
    }

    MT_SECTION("TC_COM_050: NULL 포인터 -> E_NOT_OK");
    {
        Std_ReturnType ret = Com_ReceiveSignal(COM_SIG_OverrideMode, NULL);
        MT_CHECK(ret == E_NOT_OK, "NULL 포인터: E_NOT_OK");
    }

    MT_SUMMARY();
}

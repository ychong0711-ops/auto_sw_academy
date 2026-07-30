/*
 * [EX4-2] AES-128 / AES-CMAC / SecOC 테스트
 * 요구사항 추적: [SRS_SEC_010] AES FIPS 벡터, [SRS_SEC_020] CMAC RFC4493 벡터,
 *               [SRS_SECOC_010] 보호 생성, [SRS_SECOC_020] 검증/재전송 방어
 */
#include "mini_test.h"
#include <string.h>
#include "aes_mini.h"
#include "cmac.h"
#include "secoc.h"

static int hex_eq(const uint8* got, const uint8* want, uint8 n)
{
    for (uint8 i = 0u; i < n; i++) { if (got[i] != want[i]) { return 0; } }
    return 1;
}

int main(void)
{
    MT_SECTION("TC_SEC_010: AES-128 FIPS-197 벡터");
    {
        const uint8 key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                               0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
        const uint8 pt[16]  = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                               0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
        const uint8 want[16]= {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                               0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
        uint8 ct[16];
        Aes128_EncryptBlock(key, pt, ct);
        MT_CHECK(hex_eq(ct, want, 16u), "AES 암호문 = 69c4...b4c55a");
    }

    MT_SECTION("TC_SEC_020: AES-CMAC RFC 4493 벡터");
    {
        const uint8 key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                               0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
        const uint8 wantEmpty[16] = {0xbb,0x1d,0x69,0x29,0xe9,0x59,0x37,0x28,
                                     0x7f,0xa3,0x7d,0x12,0x9b,0x75,0x67,0x46};
        const uint8 msg[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
                               0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
        const uint8 wantMsg[16] = {0x07,0x0a,0x16,0xb4,0x6b,0x4d,0x41,0x44,
                                   0xf7,0x9b,0xdd,0x9d,0xd0,0x4a,0x28,0x7c};
        uint8 mac[16];
        (void)Cmac_Calculate(key, 0, 0u, mac);
        MT_CHECK(hex_eq(mac, wantEmpty, 16u), "Example1: 빈 메시지");
        (void)Cmac_Calculate(key, msg, 16u, mac);
        MT_CHECK(hex_eq(mac, wantMsg, 16u), "Example2: 128bit 메시지");
    }

    MT_SECTION("TC_SECOC_010/020: PDU 인증 + 재전송/위조 방어");
    {
        const uint8 key[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                               0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
        SecOC_TxCtx tx; SecOC_RxCtx rx;
        uint8 auth[2], secured[8], out[2];
        uint8 f1[8], f2[8], f3[8];

        SecOC_InitTx(&tx, 0x0100u, 2u, key);
        SecOC_InitRx(&rx, 0x0100u, 2u, key);

        auth[0] = 0x00u; auth[1] = 0xC8u;                      /* speed 200 */
        MT_CHECK(SecOC_Secure(&tx, auth, f1) == E_OK, "보호 프레임#1 생성");
        MT_CHECK(SecOC_Verify(&rx, f1, out) == E_OK && out[1] == 0xC8u,
                 "정상 프레임 인증 통과");

        MT_CHECK(SecOC_Verify(&rx, f1, out) == E_NOT_OK, "같은 프레임 재전송 -> 거부");
        MT_CHECK(rx.replayFailCount == 1u, "재전송 공격 카운터 +1");

        auth[1] = 0xC9u;
        (void)SecOC_Secure(&tx, auth, f2);
        MT_CHECK(SecOC_Verify(&rx, f2, out) == E_OK, "최신 프레임#2 통과");

        (void)SecOC_Secure(&tx, auth, f3);
        f3[7] ^= 0x01u;                                        /* MAC 오염 */
        MT_CHECK(SecOC_Verify(&rx, f3, out) == E_NOT_OK, "MAC 오염 프레임 거부");

        SecOC_RxCtx evil;                                       /* 키를 모르는 공격자 */
        const uint8 wrongKey[16] = {0};
        SecOC_InitRx(&evil, 0x0100u, 2u, wrongKey);
        (void)SecOC_Secure(&tx, auth, secured);
        MT_CHECK(SecOC_Verify(&evil, secured, out) == E_NOT_OK, "키 불일치 거부");
        MT_CHECK(evil.verifyFailCount == 1u, "인증 실패 카운터 +1");
    }

    MT_SUMMARY();
}

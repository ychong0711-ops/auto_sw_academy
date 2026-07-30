/*
 * cmac.c - AES-CMAC (RFC 4493): 서브키 K1/K2 생성 + CBC-MAC
 */
#include "cmac.h"
#include "aes_mini.h"

/* 128비트 값 1비트 좌측 시프트 + 최상위비트 carry 시 0x87 반영 (GF 곱하기 x) */
static void subkey_double(const uint8 in[16], uint8 out[16])
{
    uint8 carry = 0u;
    for (int i = 15; i >= 0; i--) {
        uint8 b = in[i];
        out[i] = (uint8)((uint8)(b << 1u) | carry);
        carry = (uint8)((b >> 7u) & 1u);
    }
    if (carry != 0u) { out[15] ^= 0x87u; }
}

uint8 Cmac_Calculate(const uint8 key[16], const uint8* msg, uint32 len, uint8 macOut[16])
{
    uint8 L[16], K1[16], K2[16], X[16];
    uint8 last[16];
    uint8 isComplete, nBlocks;

    if ((key == 0) || (macOut == 0)) { return E_NOT_OK; }
    if ((len > 0u) && (msg == 0)) { return E_NOT_OK; }

    {   /* L = AES_K(0^128) */
        uint8 zero[16] = { 0u };
        Aes128_EncryptBlock(key, zero, L);
    }
    subkey_double(L, K1);
    subkey_double(K1, K2);

    /* 마지막 블록 처리: 완전 블록이면 K1, 아니면 패딩 후 K2 */
    if ((len > 0u) && ((len % 16u) == 0u)) {
        isComplete = 1u;
        nBlocks = (uint8)(len / 16u);
    } else {
        isComplete = 0u;
        nBlocks = (uint8)((len / 16u) + 1u);
    }
    if (len == 0u) { nBlocks = 1u; }

    for (uint8 i = 0u; i < 16u; i++) { last[i] = 0u; }
    if (isComplete != 0u) {
        for (uint8 i = 0u; i < 16u; i++) { last[i] = msg[(nBlocks - 1u) * 16u + i] ^ K1[i]; }
    } else {
        uint32 remain = (nBlocks == 1u) ? len : (len - (uint32)(nBlocks - 1u) * 16u);
        for (uint32 i = 0u; i < remain; i++) { last[i] = msg[(nBlocks - 1u) * 16u + i]; }
        last[remain] = 0x80u;                       /* ISO/IEC 9797-1 padding */
        for (uint8 i = 0u; i < 16u; i++) { last[i] ^= K2[i]; }
    }

    /* CBC-MAC: X_i = AES_K(X_{i-1} ^ M_i ...), 마지막은 위에서 만든 last 사용 */
    for (uint8 i = 0u; i < 16u; i++) { X[i] = 0u; }
    for (uint8 b = 1u; b < nBlocks; b++) {
        uint8 Y[16];
        for (uint8 i = 0u; i < 16u; i++) { Y[i] = (uint8)(X[i] ^ msg[(b - 1u) * 16u + i]); }
        Aes128_EncryptBlock(key, Y, X);
    }
    {
        uint8 Y[16];
        for (uint8 i = 0u; i < 16u; i++) { Y[i] = (uint8)(X[i] ^ last[i]); }
        Aes128_EncryptBlock(key, Y, macOut);
    }
    return E_OK;
}

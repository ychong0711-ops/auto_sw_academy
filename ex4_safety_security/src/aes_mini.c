/*
 * aes_mini.c - AES-128 (FIPS-197) 10 라운드
 * 상태는 column-major: state[r + 4*c] = in[r + 4c] (FIPS 규정 그대로)
 * 검증 벡터(FIPS-197 부록B):
 *   key 000102...0f, pt 00112233...ff -> ct 69c4e0d86a7b0430d8cdb78070b4c55a
 */
#include "aes_mini.h"

static uint8 s_sbox[256];
static uint8 s_ready = 0u;

static uint8 xtime(uint8 x)
{
    return (uint8)((uint8)(x << 1u) ^ (((x >> 7u) & 1u) != 0u ? 0x1Bu : 0u));
}

static uint8 gmul(uint8 a, uint8 b)   /* GF(2^8) 곱셈 */
{
    uint8 p = 0u;
    for (uint8 i = 0u; i < 8u; i++) {
        if ((b & 1u) != 0u) { p ^= a; }
        a = xtime(a);
        b = (uint8)(b >> 1u);
    }
    return p;
}

static uint8 rotl8(uint8 x, uint8 n)
{
    return (uint8)((uint8)(x << n) | (uint8)(x >> (8u - n)));
}

static void build_sbox(void)
{
    if (s_ready != 0u) { return; }
    for (uint16 i = 0u; i < 256u; i++) {
        uint8 inv = 0u;
        if (i != 0u) {
            /* GF 역원 = a^254 */
            inv = 1u;
            for (uint16 e = 0u; e < 254u; e++) { inv = gmul(inv, (uint8)i); }
        }
        /* 아핀 변환: s = inv ^ rotl(inv,1) ^ rotl(inv,2) ^ rotl(inv,3) ^ rotl(inv,4) ^ 0x63 */
        s_sbox[i] = (uint8)(inv ^ rotl8(inv, 1u) ^ rotl8(inv, 2u)
                                 ^ rotl8(inv, 3u) ^ rotl8(inv, 4u) ^ 0x63u);
    }
    s_ready = 1u;
}

static void sub_bytes(uint8 s[16])
{
    for (uint8 i = 0u; i < 16u; i++) { s[i] = s_sbox[s[i]]; }
}

static void shift_rows(uint8 s[16])
{
    uint8 t[16];
    for (uint8 r = 0u; r < 4u; r++) {
        for (uint8 c = 0u; c < 4u; c++) {
            t[(uint8)(r + 4u * c)] = s[(uint8)(r + 4u * ((uint8)(c + r) & 3u))];
        }
    }
    for (uint8 i = 0u; i < 16u; i++) { s[i] = t[i]; }
}

static void mix_columns(uint8 s[16])
{
    for (uint8 c = 0u; c < 4u; c++) {
        uint8* col = &s[c * 4u];
        uint8 s0 = col[0], s1 = col[1], s2 = col[2], s3 = col[3];
        col[0] = (uint8)(gmul(s0, 2u) ^ gmul(s1, 3u) ^ s2 ^ s3);
        col[1] = (uint8)(s0 ^ gmul(s1, 2u) ^ gmul(s2, 3u) ^ s3);
        col[2] = (uint8)(s0 ^ s1 ^ gmul(s2, 2u) ^ gmul(s3, 3u));
        col[3] = (uint8)(gmul(s0, 3u) ^ s1 ^ s2 ^ gmul(s3, 2u));
    }
}

static void add_round_key(uint8 s[16], const uint8* rk_16)
{
    for (uint8 i = 0u; i < 16u; i++) { s[i] ^= rk_16[i]; }
}

void Aes128_EncryptBlock(const uint8 key[16], const uint8 in[16], uint8 out[16])
{
    uint8 rk[176];
    uint8 state[16];
    uint8 rcon = 1u;

    build_sbox();

    /* ---- 키 스케줄 (176바이트 = 11 라운드 x 16) ---- */
    for (uint8 i = 0u; i < 16u; i++) { rk[i] = key[i]; }
    for (uint16 i = 16u; i < 176u; i += 4u) {
        uint8 t[4];
        t[0] = rk[i - 4u]; t[1] = rk[i - 3u]; t[2] = rk[i - 2u]; t[3] = rk[i - 1u];
        if ((i % 16u) == 0u) {
            uint8 tmp = t[0];                       /* RotWord */
            t[0] = t[1]; t[1] = t[2]; t[2] = t[3]; t[3] = tmp;
            for (uint8 j = 0u; j < 4u; j++) { t[j] = s_sbox[t[j]]; }   /* SubWord */
            t[0] ^= rcon;
            rcon = xtime(rcon);                     /* Rcon: 1,2,4,...,0x1B,... */
        }
        for (uint8 j = 0u; j < 4u; j++) { rk[i + (uint16)j] = (uint8)(rk[i - 16u + (uint16)j] ^ t[j]); }
    }

    for (uint8 i = 0u; i < 16u; i++) { state[i] = in[i]; }
    add_round_key(state, rk);

    for (uint8 round = 1u; round <= 9u; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &rk[round * 16u]);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, &rk[160u]);

    for (uint8 i = 0u; i < 16u; i++) { out[i] = state[i]; }
}

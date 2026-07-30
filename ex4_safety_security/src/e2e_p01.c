/*
 * e2e_p01.c - Profile 1 보호/검증 구현
 * 실제 AUTOSAR E2E 라이브러리는 Rte_E2ETransform/수동 호출로 동작하며
 * Status 는 E2E transformer 오류로 매핑됨. 여기선 개념 재현에 집중.
 */
#include "e2e_p01.h"

static uint8 compute_crc(const E2E_P01Config* cfg, const uint8* data, uint8 len)
{
    uint8 crc;
    /* 임시 버퍼에 [DataID + data(CRC 바이트 자리는 제외)] 를 붙여 한 번에 계산 */
    uint8 tmp[16];
    uint8 w = 0u;
    tmp[w++] = (uint8)(cfg->dataId & 0xFFu);        /* DataID low  */
    tmp[w++] = (uint8)(cfg->dataId >> 8u);          /* DataID high */
    for (uint8 i = 0u; i < len && w < (uint8)sizeof(tmp); i++) {
        if (i == 1u) { continue; }                  /* CRC 바이트 자리는 제외 */
        tmp[w++] = data[i];
    }
    crc = Crc8_J1850(tmp, (uint32)w);
    return crc;
}

void E2E_P01Protect(const E2E_P01Config* cfg, E2E_P01State* st, uint8* data, uint8 len)
{
    data[0] = (uint8)((data[0] & 0xF0u) | (st->txCounter & 0x0Fu));  /* 카운터 쓰기 */
    data[1] = 0u;                                                     /* CRC 위치 클리어 후 계산 */
    data[1] = compute_crc(cfg, data, len);
    st->txCounter = (uint8)((st->txCounter + 1u) % (E2E_COUNTER_MAX + 1u));
}

E2E_P01Status E2E_P01Check(const E2E_P01Config* cfg, E2E_P01State* st,
                           const uint8* data, uint8 len)
{
    uint8 rxCrc, calcCrc, rxCounter, delta;
    E2E_P01Status status;

    rxCrc = data[1];
    calcCrc = compute_crc(cfg, data, len);
    if (rxCrc != calcCrc) {
        return E2E_P01_ERROR;                     /* 데이터 오염 */
    }

    rxCounter = (uint8)(data[0] & 0x0Fu);

    if (st->rxWaitForFirst != 0u) {
        st->rxWaitForFirst = 0u;
        st->rxLastCounter = rxCounter;
        return E2E_P01_INITIAL;
    }

    if (rxCounter >= st->rxLastCounter) { delta = (uint8)(rxCounter - st->rxLastCounter); }
    else                                { delta = (uint8)(rxCounter + (E2E_COUNTER_MAX + 1u) - st->rxLastCounter); }

    if (delta == 0u)      { status = E2E_P01_REPEATED;            return status; }
    if (delta == 1u)      { status = E2E_P01_OK; }
    else if (delta <= cfg->maxDeltaCounter) { status = E2E_P01_OKSOMELOST; }
    else                  { status = E2E_P01_WRONGSEQUENCE; }

    st->rxLastCounter = rxCounter;
    return status;
}

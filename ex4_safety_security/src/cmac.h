#ifndef CMAC_H
#define CMAC_H
/*
 * cmac.h - AES-CMAC (RFC 4493) 메시지 인증 코드
 * SecOC 에서 PDU 의 "서명"을 만드는 표준 알고리즘.
 * 검증 벡터(RFC 4493):
 *   K=2b7e151628aed2a6abf7158809cf4f3c, M=""      -> bb1d6929e95937287fa37d129b756746
 *   M=6bc1bee22e409f96e93d7e117393172a           -> 070a16b46b4d4144f79bdd9dd04a287c
 */
#include "Std_Types.h"

uint8 Cmac_Calculate(const uint8 key[16], const uint8* msg, uint32 len, uint8 macOut[16]);

#endif /* CMAC_H */

#ifndef AES_MINI_H
#define AES_MINI_H
/*
 * aes_mini.h - AES-128 암호화(블록 1개) 교육용 구현
 * SecOC 에서 쓰는 CMAC 의 기반이 되는 블록 암호.
 * S-Box 는 테이블 대신 GF(2^8) 역원+아핀 변환으로 "계산"한다 (테이블 오타 방지).
 * 주의: 교육용. 실무에서는 MCU 의 하드웨어 암호 가속기/HSM 또는 검증된 라이브러리(CryIf) 사용.
 */
#include "Std_Types.h"

void Aes128_EncryptBlock(const uint8 key[16], const uint8 in[16], uint8 out[16]);

#endif /* AES_MINI_H */

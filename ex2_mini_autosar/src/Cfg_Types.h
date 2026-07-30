#ifndef CFG_TYPES_H
#define CFG_TYPES_H
/*
 * Cfg_Types.h - 설정(Generated Config) 타입 정의
 * 실제 AUTOSAR 의 모듈 Cfg/Types 헤더 역할.
 * generated 폴의 가 아니라 그 폴의 C 파일들이 "배열 데이터"를 제공한다 (tools/gen_cfg.py 생성).
 */
#include "ComStack_Types.h"

/* ---- CanIf ---- */
typedef struct { PduIdType pduId;  uint16 canId; uint8 hth; } CanIf_TxPduCfg;
typedef struct { uint16 canId;     uint16 hoh;   PduIdType rxPduId; } CanIf_RxPduCfg;

extern const CanIf_TxPduCfg CanIf_TxPduConfig[];
extern const uint16         CanIf_TxPduConfig_Size;
extern const CanIf_RxPduCfg CanIf_RxPduConfig[];
extern const uint16         CanIf_RxPduConfig_Size;

/* ---- PduR ---- */
typedef struct { PduIdType srcCom;   PduIdType dstCanIf; } PduR_TxRoute;
typedef struct { PduIdType srcCanIf; PduIdType dstCom;   } PduR_RxRoute;

extern const PduR_TxRoute PduR_TxRoutes[];
extern const uint16       PduR_TxRoutes_Size;
extern const PduR_RxRoute PduR_RxRoutes[];
extern const uint16       PduR_RxRoutes_Size;

/* ---- Com ---- */
typedef struct {
    uint8  id;          /* Com_SignalIdType 과 동일 공간 (COM_SIG_*) */
    uint8  isTx;
    uint8  pduIdx;      /* TX/RX PDU 배열 인덱스 */
    uint16 startBit;
    uint8  length;
} Com_SignalCfg;

extern const Com_SignalCfg Com_SignalConfig[];
extern const uint16        Com_SignalConfig_Size;

#endif /* CFG_TYPES_H */

/*
 * [EX1-3] 링 버퍼 - ISR <-> 메인 루프 통신의 표준 자료구조
 *
 * 실무 배경
 *  - UDS 수신 ISR이 바이트를 push 하고, 메인 루프가 pop 해서 프레임을 조립.
 *  - head(생산자)/tail(소비자)를 volatile 로: 양쪽 문맥이 서로를 갱신하므로.
 *  - "가득 참" 판정: (head+1)%N == tail  → 한 칸을 희생한다.
 *
 * 요구사항 추적: [SRS_RB_010] 가득 차면 push 실패를 반환하고 유실 카운트 기록
 *               [SRS_RB_020] FIFO 순서와 wrap-around 정합성 보장
 */
#include <stdint.h>
#include <stdbool.h>
#include "mini_test.h"

#define RB_CAPACITY 8u

typedef struct {
    uint8_t          buf[RB_CAPACITY];
    volatile uint8_t head;      /* 생산자(ISR)만 쓴다        */
    volatile uint8_t tail;      /* 소비자(메인)만 쓴다       */
    uint32_t         dropped;   /* [SRS_RB_010] 유실 통계    */
} RingBuf_t;

static void rb_init(RingBuf_t *rb) { rb->head = 0u; rb->tail = 0u; rb->dropped = 0u; }

static bool rb_push(RingBuf_t *rb, uint8_t b)   /* ISR 문맥에서 호출된다고 가정 */
{
    uint8_t next = (uint8_t)((rb->head + 1u) % RB_CAPACITY);
    if (next == rb->tail) { rb->dropped++; return false; }   /* full */
    rb->buf[rb->head] = b;
    rb->head = next;
    return true;
}

static bool rb_pop(RingBuf_t *rb, uint8_t *out)
{
    if (rb->head == rb->tail) { return false; }              /* empty */
    *out = rb->buf[rb->tail];
    rb->tail = (uint8_t)((rb->tail + 1u) % RB_CAPACITY);
    return true;
}

static uint8_t rb_count(const RingBuf_t *rb)
{
    return (uint8_t)((rb->head + RB_CAPACITY - rb->tail) % RB_CAPACITY);
}

int main(void)
{
    RingBuf_t rb;
    rb_init(&rb);
    uint8_t b = 0u;

    MT_SECTION("기본 FIFO");
    MT_CHECK(rb_push(&rb, 0x11u) && rb_push(&rb, 0x22u), "TC_RB_001 push 2바이트");
    MT_CHECK(rb_count(&rb) == 2u, "TC_RB_002 count=2");
    (void)rb_pop(&rb, &b);
    MT_CHECK(b == 0x11u, "TC_RB_003 FIFO 순서 (먼저 들어간 것이 먼저)");

    MT_SECTION("가득 참 / 유실");
    rb_init(&rb);
    for (uint8_t i = 0u; i < RB_CAPACITY; i++) { (void)rb_push(&rb, i); }
    /* 용량 8 이지만 한 칸 희생 → 7 개만 수용, 8번째 push 는 실패(유실 1건) */
    MT_CHECK(rb_count(&rb) == RB_CAPACITY - 1u, "TC_RB_004 실제 수용량 = N-1");
    MT_CHECK(!rb_push(&rb, 0xAAu), "TC_RB_005 full 에서 push 실패 반환");
    MT_CHECK(rb.dropped == 2u, "TC_RB_006 유실 카운트 누적=2 (채울 때 1 + 여기서 1)");

    MT_SECTION("wrap-around 정합성");
    rb_init(&rb);
    for (int round = 0; round < 3; round++) {          /* 여러 번 감아도 정상 */
        for (uint8_t i = 0u; i < 5u; i++) { (void)rb_push(&rb, (uint8_t)(round * 10 + i)); }
        for (uint8_t i = 0u; i < 5u; i++) {
            (void)rb_pop(&rb, &b);
            if (b != (uint8_t)(round * 10 + (int)i)) { MT_CHECK(false, "TC_RB_007 wrap 순서 깨짐"); }
        }
    }
    MT_CHECK(rb_count(&rb) == 0u, "TC_RB_007 wrap-around 후 empty 복귀");

    MT_SUMMARY();
}

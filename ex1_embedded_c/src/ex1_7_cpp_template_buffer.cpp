/*
 * [EX1-7] C++ 템플릿 링 버퍼 — RAII + 제네릭 프로그래밍
 *
 * 학습 포인트
 *  - C++ 템플릿: 타입에 독립적인 자료구조 (T, N)
 *  - RAII(Resource Acquisition Is Initialization): 생성자/소멸자가 자원 관리
 *  - AUTOSAR SWC에서 ISR↔메인루프 데이터 교환의 표준 패턴을 C++로 재구현
 *  - 실무(MISRA C++)에서는 동적 할당 금지 → 템플릿 + 정적 배열 조합이 표준
 *
 * 요구사항 추적: [SRS_RB_010] 가득 차면 push 실패 반환
 *               [SRS_RB_020] FIFO 순서와 wrap-around 정합성
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "mini_test.h"

/* 템플릿 링 버퍼: T=데이터 타입, N=용량 */
template<typename T, uint8_t N>
class RingBuffer {
public:
    RingBuffer() : m_head(0u), m_tail(0u), m_dropped(0u) {}

    /* push: 가득 차면 false + 유실 카운트 증가 */
    bool push(T value) {
        uint8_t next = static_cast<uint8_t>((m_head + 1u) % N);
        if (next == m_tail) {
            m_dropped++;
            return false;                      /* [SRS_RB_010] full */
        }
        m_buf[m_head] = value;
        m_head = next;
        return true;
    }

    /* pop: 비어있으면 false, out에 값 복사 */
    bool pop(T& out) {
        if (m_head == m_tail) {
            return false;                      /* empty */
        }
        out = m_buf[m_tail];
        m_tail = static_cast<uint8_t>((m_tail + 1u) % N);
        return true;
    }

    uint8_t count() const {
        return static_cast<uint8_t>((m_head + N - m_tail) % N);
    }

    uint32_t dropped() const { return m_dropped; }
    void     reset() { m_head = 0u; m_tail = 0u; m_dropped = 0u; }

    /* 실제 수용 가능한 최대 개수 (한 칸 희생) */
    static constexpr uint8_t capacity() { return N - 1u; }

private:
    T        m_buf[N];
    uint8_t  m_head;
    uint8_t  m_tail;
    uint32_t m_dropped;

    /* 복사 금지 (RAII: 소유권 독점) */
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
};

/* ================ 테스트 ================ */
int main(void)
{
    MT_SECTION("기본 FIFO (RingBuffer<uint8_t, 8>)");
    {
        RingBuffer<uint8_t, 8> rb;
        uint8_t b = 0u;

        MT_CHECK(rb.push(0x11u) && rb.push(0x22u),
                 "push 2바이트 성공");
        MT_CHECK(rb.count() == 2u, "count=2");

        (void)rb.pop(b);
        MT_CHECK(b == 0x11u, "FIFO 순서: 먼저 들어간 값이 먼저 나옴");

        (void)rb.pop(b);
        MT_CHECK(b == 0x22u, "두 번째 값 정합");
    }

    MT_SECTION("가득 참 / 유실 (SRS_RB_010)");
    {
        RingBuffer<uint8_t, 8> rb;

        /* 용량 8, 한 칸 희생 → 7개까지만 push 성공 */
        for (uint8_t i = 0u; i < 8u; i++) {
            (void)rb.push(i);
        }
        /* uint8_t,8 콤마가 전처리기 인자 분할을 유발하므로 임시 변수 우회 */
        uint8_t cnt = rb.count();
        uint8_t cap = RingBuffer<uint8_t, 8>::capacity();
        MT_CHECK(cnt == cap, "실제 수용량 = N-1 (7)");
        MT_CHECK(!rb.push(0xAAu), "full에서 push 실패 반환");
        MT_CHECK(rb.dropped() == 2u,
                 "유실 카운트=2 (채울 때 1 + 여기서 1)");
    }

    MT_SECTION("wrap-around 정합성 (SRS_RB_020)");
    {
        RingBuffer<int, 8> rb;  /* int 타입으로도 동작 확인 */

        for (int round = 0; round < 3; round++) {
            for (int i = 0; i < 5; i++) {
                (void)rb.push(round * 10 + i);
            }
            for (int i = 0; i < 5; i++) {
                int val = 0;
                (void)rb.pop(val);
                MT_CHECK(val == round * 10 + i,
                         "wrap-around 순서 정합");
            }
        }
        MT_CHECK(rb.count() == 0, "wrap 후 empty 복귀");
    }

    MT_SECTION("다양한 타입: RingBuffer<uint16_t, 16>");
    {
        RingBuffer<uint16_t, 16> rb;

        for (uint16_t i = 0u; i < 15u; i++) {
            MT_CHECK(rb.push(i), "타입 독립 push");
        }
        MT_CHECK(rb.count() == 15u, "uint16_t 15개 저장");
        MT_CHECK(!rb.push(999u), "가득 참 감지");

        /* 순서 검증 */
        for (uint16_t i = 0u; i < 15u; i++) {
            uint16_t v = 0u;
            (void)rb.pop(v);
            MT_CHECK(v == i, "uint16_t FIFO 순서 정합");
        }
    }

    MT_SUMMARY();
}

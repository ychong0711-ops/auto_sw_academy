/*
 * [EX1-8] C++ 고정소수점 클래스 — 연산자 오버로딩 + 타입 안전성
 *
 * 학습 포인트
 *  - C++ 연산자 오버로딩: Q16.16 타입이 내장 타입처럼 동작
 *  - 생성자 변환: int/double에서 Q16으로 암시적 변환 제어 (explicit)
 *  - 64비트 중간 결과로 오버플로우 방지 (SRS_FX_010 계승)
 *  - AUTOSAR SWC에서 물리량 연산에 fixed-point가 사용되는 배경 재현
 *  - 컴파일 타임 타입 안전성: int12_raw_t, Q16 등으로 단위 실수 방지
 *
 * MISRA C++ 규칙 참고:
 *  - Rule 5-0-4: 암시적 정수 변환 금지 → explicit 생성자 + 명시적 캐스트
 *  - Rule 5-19-1: 연산자 오버로딩은 자연스러운 의미에 한정
 */

#include <cstdint>
#include <cstdio>
#include "mini_test.h"

/* ================ Q16.16 고정소수점 클래스 ================ */
class Q16 {
public:
    /* 1바이트 단위 -> 자주 쓰는 변환 */
    static constexpr int FRAC_BITS = 16;
    static constexpr int64_t SCALE = static_cast<int64_t>(1) << FRAC_BITS;

    /* 기본 생성자 + int에서 변환 (explicit: 암시적 변환 금지) */
    explicit Q16() : m_raw(0) {}
    explicit Q16(int val) : m_raw(val << FRAC_BITS) {}
    explicit Q16(double val) : m_raw(static_cast<int32_t>(val * SCALE + 0.5)) {}

    /* raw 값으로부터 직접 생성 (내부용) */
    static Q16 fromRaw(int32_t raw) { Q16 q; q.m_raw = raw; return q; }

    /* 접근자 */
    int32_t raw() const { return m_raw; }
    double  toDouble() const { return static_cast<double>(m_raw) / SCALE; }
    int     toInt() const { return m_raw >> FRAC_BITS; }

    /* ---- 연산자 오버로딩 ---- */

    /* [SRS_FX_010] 곱셈: 64비트 중간 결과 사용 */
    Q16 operator*(const Q16& other) const {
        int64_t t = (static_cast<int64_t>(m_raw) * other.m_raw) >> FRAC_BITS;
        return fromRaw(static_cast<int32_t>(t));
    }

    /* 나눗셈 */
    Q16 operator/(const Q16& other) const {
        return fromRaw(static_cast<int32_t>(
            (static_cast<int64_t>(m_raw) << FRAC_BITS) / other.m_raw));
    }

    /* 덧셈/뺄셈 */
    Q16 operator+(const Q16& other) const {
        return fromRaw(m_raw + other.m_raw);
    }
    Q16 operator-(const Q16& other) const {
        return fromRaw(m_raw - other.m_raw);
    }

    /* 비교 연산자 */
    bool operator==(const Q16& other) const { return m_raw == other.m_raw; }
    bool operator!=(const Q16& other) const { return m_raw != other.m_raw; }
    bool operator<(const Q16& other) const  { return m_raw <  other.m_raw; }
    bool operator>(const Q16& other) const  { return m_raw >  other.m_raw; }

private:
    int32_t m_raw;
};

/* ================ 물리량 <-> RAW 변환 (Com signal factor/offset) ================ */
struct SignalConversion {
    double factor;
    double offset;

    uint16_t physToRaw(double phys) const {
        return static_cast<uint16_t>((phys - offset) / factor + 0.5);
    }
    double rawToPhys(uint16_t raw) const {
        return static_cast<double>(raw) * factor + offset;
    }
};

/* ================ 테스트 ================ */
int main(void)
{
    MT_SECTION("Q16.16 생성 및 기본 연산");
    {
        Q16 a(3);           /* 3.0 */
        Q16 b = Q16(2);     /* 2.0 */

        MT_CHECK((a / b).toInt() == 1,     "3/2 = 1 (toInt 절삭)");
        MT_CHECK((a + b).toInt() == 5,     "3+2 = 5");
        MT_CHECK((a - Q16(1)).toInt() == 2, "3-1 = 2");

        /* 1.5 * 4 = 6.0 */
        Q16 c = Q16(3) / Q16(2);    /* 1.5 */
        Q16 d = Q16(4);
        MT_CHECK((c * d).toInt() == 6, "1.5*4 = 6.0");
    }

    MT_SECTION("64비트 중간 결과로 오버플로우 방지 (SRS_FX_010)");
    {
        /* (150<<16)*(150<<16) = 9.7e13 → int32로 넘침, int64는 안전 */
        Q16 big(150);
        Q16 result = big * big;
        Q16 expected = Q16(22500);
        MT_CHECK(result == expected, "150*150 = 22500 (int64 중간 결과)");
    }

    MT_SECTION("Double 변환");
    {
        Q16 a(0);
        Q16 b(1);
        Q16 c = Q16(3) / Q16(2);       /* 1.5 */

        MT_CHECK(a.toDouble() >= -0.001 && a.toDouble() <= 0.001,
                 "0.0 근사값");
        MT_CHECK(b.toDouble() >= 0.999 && b.toDouble() <= 1.001,
                 "1.0 근사값");
        MT_CHECK(c.toDouble() >= 1.499 && c.toDouble() <= 1.501,
                 "1.5 근사값");
    }

    MT_SECTION("물리량 <-> raw (AUTOSAR ComSignal factor/offset 패턴)");
    {
        /* 차량 전압: factor=0.1 V/bit, offset=0, 12-bit range */
        SignalConversion battery = {0.1, 0.0};

        uint16_t raw = battery.physToRaw(12.7);
        MT_CHECK(raw == 127u, "12.7V -> raw 127");

        double phys = battery.rawToPhys(127u);
        MT_CHECK(phys > 12.69 && phys < 12.71, "raw 127 -> 12.7V 복원");
    }

    MT_SECTION("비교 연산자");
    {
        Q16 a(10);
        Q16 b(20);

        MT_CHECK(a < b,  "10 < 20");
        MT_CHECK(b > a,  "20 > 10");
        MT_CHECK(a != b, "10 != 20");
        MT_CHECK(a == Q16(10), "10 == 10");
    }

    MT_SUMMARY();
}

/*
 * [EX1-6] C++ OOP 상태기계 — 다형성을 활용한 상태 패턴
 *
 * 학습 포인트
 *  - C++ OOP: 클래스, 상속, 가상 함수, 다형성
 *  - 상태 패턴(State Pattern): 각 상태가 객체로 분리되어 전이 로직이 상태별로 캡슐화
 *  - AUTOSAR SWC 도 "상태" 단위로 동작(Dcm 세션, CanSM 통신 상태)
 *  - 주의: 학습용 new/delete 사용. 실무 AUTOSAR/MISRA C++에서는 동적 할당 대신
 *          정적 풀(static pool) 또는 미리 할당된 상태 인스턴스 교체 방식 사용.
 *
 * 요구사항 추적: [SRS_SM_010] 정의되지 않은 이벤트 무시
 *               [SRS_SM_020] 상태별 출력 결정
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "mini_test.h"

/* ================ OOP 상태 패턴 ================ */
enum class IgnEvent : uint8_t {
    KEY_OFF = 0,
    KEY_ACC,
    KEY_ON,
    START_BTN,
    ENGINE_STARTED,
    ENGINE_STALLED,
    INVALID
};

struct IgnOutput {
    uint8_t acc;   /* ACC 릴레이 */
    uint8_t ign;   /* IGN 릴레이 */
    uint8_t str;   /* 스타터 */
};

/* 추상 상태 베이스 */
class IgnState {
public:
    virtual ~IgnState() = default;
    virtual IgnState* handleEvent(IgnEvent e, IgnOutput& out) = 0;
    virtual const char* name() const = 0;

protected:
    static void setOutput(IgnOutput& out, uint8_t acc, uint8_t ign, uint8_t str) {
        out.acc = acc; out.ign = ign; out.str = str;
    }
};

/* ================ 구체 상태 클래스들 (인라인 정의) ================ */

/* 상태 전이 시 상호 참조가 필요하므로, 모든 handleEvent()를
   클래스 외부(inline namespace)에 정의한다. */

class OffState : public IgnState {
public:
    IgnState* handleEvent(IgnEvent e, IgnOutput& out) override;
    const char* name() const override { return "OFF"; }
};

class AccState : public IgnState {
public:
    IgnState* handleEvent(IgnEvent e, IgnOutput& out) override;
    const char* name() const override { return "ACC"; }
};

class OnState : public IgnState {
public:
    IgnState* handleEvent(IgnEvent e, IgnOutput& out) override;
    const char* name() const override { return "ON"; }
};

class CrankState : public IgnState {
public:
    IgnState* handleEvent(IgnEvent e, IgnOutput& out) override;
    const char* name() const override { return "CRANK"; }
};

class RunState : public IgnState {
public:
    IgnState* handleEvent(IgnEvent e, IgnOutput& out) override;
    const char* name() const override { return "RUN"; }
};

/* ---- 각 상태의 전이 로직 (클래스 외부 정의) ---- */

/* OffState: KEY_ACC 만 ACC로 전이 */
IgnState* OffState::handleEvent(IgnEvent e, IgnOutput& out) {
    if (e == IgnEvent::KEY_ACC) {
        setOutput(out, 1u, 0u, 0u);
        return new AccState();
    }
    setOutput(out, 0u, 0u, 0u);
    return this;  /* [SRS_SM_010] 무효 이벤트는 상태 유지 */
}

/* AccState: KEY_ON→ON, KEY_OFF→OFF */
IgnState* AccState::handleEvent(IgnEvent e, IgnOutput& out) {
    if (e == IgnEvent::KEY_ON) {
        setOutput(out, 1u, 1u, 0u);
        return new OnState();
    }
    if (e == IgnEvent::KEY_OFF) {
        setOutput(out, 0u, 0u, 0u);
        return new OffState();
    }
    setOutput(out, 1u, 0u, 0u);
    return this;
}

/* OnState: START_BTN→CRANK, KEY_ACC→ACC */
IgnState* OnState::handleEvent(IgnEvent e, IgnOutput& out) {
    if (e == IgnEvent::START_BTN) {
        setOutput(out, 1u, 1u, 1u);
        return new CrankState();
    }
    if (e == IgnEvent::KEY_ACC) {
        setOutput(out, 1u, 0u, 0u);
        return new AccState();
    }
    setOutput(out, 1u, 1u, 0u);
    return this;
}

/* CrankState: ENGINE_STARTED→RUN, KEY_ACC→ACC */
IgnState* CrankState::handleEvent(IgnEvent e, IgnOutput& out) {
    if (e == IgnEvent::ENGINE_STARTED) {
        setOutput(out, 1u, 1u, 0u);
        return new RunState();
    }
    if (e == IgnEvent::KEY_ACC) {
        setOutput(out, 1u, 0u, 0u);
        return new AccState();
    }
    setOutput(out, 1u, 1u, 1u);
    return this;
}

/* RunState: ENGINE_STALLED→ON, KEY_ACC→ACC, KEY_OFF→OFF */
IgnState* RunState::handleEvent(IgnEvent e, IgnOutput& out) {
    if (e == IgnEvent::ENGINE_STALLED) {
        setOutput(out, 1u, 1u, 0u);
        return new OnState();
    }
    if (e == IgnEvent::KEY_ACC) {
        setOutput(out, 1u, 0u, 0u);
        return new AccState();
    }
    if (e == IgnEvent::KEY_OFF) {
        setOutput(out, 0u, 0u, 0u);
        return new OffState();
    }
    setOutput(out, 1u, 1u, 0u);
    return this;
}

/* ================ RAII 컨트롤러 ================ */
class IgnitionController {
public:
    IgnitionController() : m_state(new OffState()) {}
    ~IgnitionController() { delete m_state; }

    /* 복사/대입 금지 (RAII: 소유권 독점) */
    IgnitionController(const IgnitionController&) = delete;
    IgnitionController& operator=(const IgnitionController&) = delete;

    void handleEvent(IgnEvent e) {
        IgnOutput out;
        IgnState* next = m_state->handleEvent(e, out);
        if (next != m_state) {
            delete m_state;
            m_state = next;
        }
        m_output = out;
    }

    const IgnOutput& output() const { return m_output; }
    const char* stateName() const { return m_state->name(); }

private:
    IgnState* m_state;
    IgnOutput m_output = {0u, 0u, 0u};
};

/* ================ 테스트 ================ */

/* strcmp 비교 헬퍼 */
static int is_state(const IgnitionController& ign, const char* want) {
    return std::strcmp(ign.stateName(), want) == 0;
}

int main(void)
{
    MT_SECTION("정상 시동 시퀀스");
    {
        IgnitionController ign;

        ign.handleEvent(IgnEvent::KEY_ACC);
        MT_CHECK(is_state(ign, "ACC"), "OFF->ACC 상태 전이");

        ign.handleEvent(IgnEvent::KEY_ON);
        MT_CHECK(is_state(ign, "ON"), "ACC->ON 상태 전이");

        ign.handleEvent(IgnEvent::START_BTN);
        MT_CHECK(ign.output().str == 1u, "CRANK: 스타터 ON");
        MT_CHECK(is_state(ign, "CRANK"), "ON->CRANK 상태 전이");

        ign.handleEvent(IgnEvent::ENGINE_STARTED);
        MT_CHECK(ign.output().str == 0u && ign.output().ign == 1u,
                 "RUN: 스타터 OFF, IGN 유지");
        MT_CHECK(is_state(ign, "RUN"), "RUN 상태 확인");
    }

    MT_SECTION("무효 이벤트 무시 (SRS_SM_010)");
    {
        IgnitionController ign;
        ign.handleEvent(IgnEvent::START_BTN);  /* OFF 상태서 시동버튼 = 무시 */
        MT_CHECK(is_state(ign, "OFF"), "OFF->START 무시: OFF 유지");
        MT_CHECK(ign.output().str == 0u, "스타터 미동작");
    }

    MT_SECTION("엔진 정지 → ON 복귀");
    {
        IgnitionController ign;
        ign.handleEvent(IgnEvent::KEY_ACC);
        ign.handleEvent(IgnEvent::KEY_ON);
        ign.handleEvent(IgnEvent::START_BTN);
        ign.handleEvent(IgnEvent::ENGINE_STARTED);
        ign.handleEvent(IgnEvent::ENGINE_STALLED);

        MT_CHECK(is_state(ign, "ON"), "STALLED->ON 복귀");
        MT_CHECK(ign.output().ign == 1u, "ON: IGN 유지 (재시동 가능)");
    }

    MT_SUMMARY();
}

# 코드 리딩 가이드

이 샘플의 C++를 읽는 순서와, 각 파일에서 봐야 할 지점을 정리했습니다.
전체 게임 로직은 `Source/ProjectTF/CQB/` 안에 있고, 템플릿 파일 중 3개만 수정했습니다.

---

## 읽는 순서 (30분 코스)

| 순서 | 파일 | 줄 수 | 왜 먼저 |
|---|---|---|---|
| 1 | `CQB/CQBTypes.h` | 84 | 상태·역할·콜아웃 enum. 나머지 전부가 이걸 참조 |
| 2 | `CQB/HealthComponent.h/.cpp` | 147 | 가장 단순. 컴포넌트 패턴 감 잡기 |
| 3 | `CQB/WeaponData.h` | 101 | 무기 파라미터 전부. 주석에 단위까지 |
| 4 | `CQB/WeaponComponent.cpp` | 415 | 사격 파이프라인 한 곳. `Fire()` 하나만 봐도 절반 |
| 5 | `CQB/EnemyAIController.h` | 255 | 상태머신 구조. 헤더만 봐도 설계가 보임 |
| 6 | `CQB/EnemyAIController.cpp` | 812 | 본체. 아래 "읽는 법" 참고 |
| 7 | `CQB/SquadManager.cpp` | 222 | 역할 배분 / 측면 지점 / 콜아웃 |

---

## 1. UWeaponComponent — 사격 파이프라인

**`UWeaponComponent::Fire()` 한 함수가 전부입니다.** 순서대로:

```
CanFire()          재장전 중 / 쿨다운 / 사망 체크
  ↓
탄약 0이면         → Reload() 걸고 종료
  ↓
GetViewPoint()     카메라(플레이어) 또는 폰 시점(AI)에서 시작점·방향
  ↓
FMath::VRandCone() AimSpreadHalfAngle 만큼 탄 퍼짐
  ↓
LineTraceSingleByChannel(ECC_Visibility)
  ↓
ApplyPointDamage() → AActor::TakeDamage → OnTakeAnyDamage → UHealthComponent
  ↓
ApplyRecoil()      컨트롤러 회전에 가산, AccumulatedRecoil에 기록
  ↓
ReportNoiseEvent() AI 청각에 총성 전달
  ↓
SetTimer(RefireTimerHandle)  FireRate 기반 다음 발사
```

**볼 만한 지점**
- `GetViewPoint()` — 플레이어는 카메라, AI는 `GetPawnViewLocation()`. 같은 컴포넌트를 둘 다 쓰는 핵심
- `ApplyRecoil()` / `RecoverRecoil()` — 반동을 컨트롤러 회전에 직접 더하고, 미발사 시 같은 양을 빼서 되돌림
- `GetWeaponData()` — 데이터 에셋이 없으면 런타임 기본값 생성. 에셋 없이도 동작하는 이유

---

## 2. AEnemyAIController — 상태머신

### 구조

```cpp
Tick()
 ├─ UpdateSenses()            시야 유지 판정 (LineOfSightTo)
 ├─ RetryFailedMove()         경로 실패 시 재시도
 ├─ UpdateGlobalTransitions() 상태 무관 규칙 (시야 상실, 플레이어 사망)
 ├─ UpdateState(CurrentState) 현재 상태의 Update
 └─ DrawStateDebug()          머리 위 텍스트

SetState(New)
 ├─ ExitState(Current)
 ├─ CurrentState = New
 └─ EnterState(New)
```

`EnterState/UpdateState/ExitState`는 switch 하나로 `EnterIdle()`, `UpdateIdle()` ... 로 분기합니다.
**상태 하나를 이해하려면 그 상태의 Enter/Update/Exit 세 함수만 보면 됩니다.**

### 읽는 법

`Idle → Investigate → Engage → Cover → Suppress` 순서로 6쌍을 읽으면 전체가 보입니다.
각 상태는 30줄을 넘지 않습니다.

### 주의해서 볼 3곳

**(1) `UpdateGlobalTransitions()` — Flank 예외**
```cpp
if (IsInCombat() && CurrentState != EEnemyState::Flank)
```
우회는 시야를 일부러 끊는 기동이라 "3초 시야 상실 → Investigate" 규칙에서 뺐습니다.
안 빼면 측면 복도(편도 22m, 6초) 중간에 모든 flank가 취소됩니다.

**(2) `HasReachedGoal()` + `RetryFailedMove()`**
경로 요청 실패를 "도착"으로 처리하면 적이 그냥 포기합니다. NavMesh가 생성 중일 때 실제로 그랬습니다.
지금은 재시도 예산(`MaxMoveRetries`)을 다 쓸 때까지 도착으로 치지 않습니다.

**(3) `FindCoverPointFallback()`**
EQS 에셋 없이 동작하는 엄폐 탐색입니다. NavMesh에서 24지점 샘플링 →
플레이어 눈높이에서 후보 지점으로 트레이스가 **막히는** 곳만 통과 → 자기 위치에서 가장 가까운 곳 선택.

---

## 3. ASquadManager — 분대

세 함수만 보면 됩니다.

| 함수 | 하는 일 |
|---|---|
| `RequestRole()` | 이미 역할 가진 인원 수 기준 배정. 짝수번째 Suppressor, 홀수번째 Flanker(좌우 교대) |
| `GetFlankPoint()` | 플레이어→적 벡터를 ±90도 회전, 거리 600, NavMesh 투영. 실패 시 반대쪽 → 랜덤 순 |
| `NotifyEnemyDied()` | "Man down!" → 전원 역할 초기화 → 생존자 재요청 |

`ReassignRoles()`가 **2패스**인 이유: 1패스로 하면 먼저 재요청한 적이 아직 옛 역할을 든
다른 적들을 세어버립니다. 전부 비운 뒤 재요청해야 맞습니다.

---

## 4. 수정한 템플릿 파일 3개

| 파일 | 무엇을 |
|---|---|
| `ProjectTFCharacter.h/.cpp` | Health/Weapon 컴포넌트, Fire·ADS·Reload·Lean 입력, `IShooterWeaponHolder` 구현(무기 비주얼) |
| `ProjectTFCameraManager.cpp` | `UpdateViewTarget()`에서 린(roll + 측면 오프셋)을 최종 POV에 적용 |
| `ProjectTFPlayerController.cpp` | `BeginPlay`에서 `ClientSetHUD(ACQBHUD)` |

**린을 왜 카메라 매니저에서 처리하나:**
카메라 컴포넌트가 `bUsePawnControlRotation = true`라 컴포넌트에 roll을 넣어도 매 프레임 덮어씁니다.
최종 POV가 결정된 뒤에 더해야 합니다.

**무기 비주얼이 왜 인터페이스를 타나:**
`AShooterWeapon`(템플릿)이 메시·손 포즈 애님 BP·발사 몽타주를 **한 세트로** 들고 있습니다.
`OnWeaponActivated()`에서 무기가 알려주는 애님 BP를 캐릭터 메시에 적용합니다.
애님을 직접 고르면 포즈와 무기 위치가 어긋납니다 (실제로 그렇게 만들었다가 고쳤습니다).

---

## 5. 설계 선택과 이유

| 선택 | 이유 |
|---|---|
| Behavior Tree 대신 직접 만든 상태머신 | 요구사항. 전이 규칙이 코드에 다 보이는 장점도 있음 |
| 플레이어와 적이 같은 컴포넌트 | 체력·무기 로직을 한 벌만 유지. AI는 `StartFire()`를 호출할 뿐 |
| 데미지를 `ApplyPointDamage` 경유 | 직접 호출하면 God 치트·데미지 타입·면역이 전부 무시됨 |
| NavMesh를 Navigation Invoker로 | 볼륨 방식으로는 타일이 0개 생성됨. 커맨드릿은 네비 빌드를 잠금 |
| 입력을 `BindKey` 직접 바인딩 | IA 에셋 없이 동작. IA를 지정하면 그쪽이 우선 |

---

## 6. 직접 돌려볼 때

```bash
# 헤드리스로 AI 전체 시퀀스 보기 (로그만)
ProjectTF.exe -game -nullrhi -unattended -stdout

# 아군 사망 → 역할 재배정 보기
ProjectTF.exe -game -nullrhi -unattended -stdout -CQBKillEnemyAfter=10

# 스크린샷
ProjectTF.exe -game -windowed -CQBScreenshotAfter=5
```

로그에서 `CQB:` 로 시작하는 줄만 보면 AI 동작이 전부 추적됩니다.

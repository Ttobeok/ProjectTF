# 코드 리딩 가이드

이 샘플의 C++를 읽는 순서와, 각 파일에서 봐야 할 지점을 정리했습니다.
게임 로직은 전부 `Source/ProjectTF/CQB/` 안에 있고, 템플릿 파일은 3개만 수정했습니다.

한 줄 요약: **적 AI와 아군 분대가 같은 상태머신을 쓰고, 진영만 다릅니다.**

---

## 읽는 순서 (40분 코스)

| 순서 | 파일 | 줄 | 왜 이 순서 |
|---|---|---|---|
| 1 | `CQBTypes.h` | 168 | 진영·상태·역할·콜아웃 enum + 진영 판별. 나머지 전부가 참조 |
| 2 | `HealthComponent.h/.cpp` | 147 | 가장 단순. 컴포넌트 패턴 감 잡기 |
| 3 | `WeaponData.h` | 101 | 무기 수치 전부. 주석에 단위까지 |
| 4 | `WeaponComponent.cpp` | 426 | 사격 파이프라인. `Fire()` 하나면 절반 |
| 5 | `EnemyAIController.h` | 297 | 상태머신 구조. 헤더만 봐도 설계가 보임 |
| 6 | `EnemyAIController.cpp` | 888 | 전투 본체 |
| 7 | `AllyAIController.cpp` | 323 | 6번을 상속해 명령 4개를 얹은 것 |
| 8 | `SquadManager.cpp` | 222 | 적 분대의 역할 배분·콜아웃 |

---

## 1. 전체 구조 한 장

```
                    ┌──────────────────────────┐
                    │  AEnemyAIController      │  인지 + 전투 상태머신
                    │  Idle/Investigate/Engage │  (Faction = Enemy)
                    │  Cover/Flank/Suppress    │
                    └───────────┬──────────────┘
                                │ 상속
                    ┌───────────▼──────────────┐
                    │  AAllyAIController       │  + 명령 4개
                    │  Follow/Hold/Stack/Clear │  (Faction = Ally)
                    └──────────────────────────┘

  AEnemyCharacter ──┬── UHealthComponent      플레이어와 공용
                    ├── UWeaponComponent      플레이어와 공용
                    ├── UWeaponVisualComponent
                    └── UNavigationInvokerComponent
        ▲
        │ 상속 (Faction만 Ally)
   AAllyCharacter
```

**같은 컴포넌트, 같은 상태머신, 진영만 다름.** 이게 설계의 전부입니다.

---

## 2. UWeaponComponent — 사격 파이프라인

**`Fire()` 한 함수가 전부입니다.**

```
CanFire()          재장전 중 / 쿨다운 / 사망 체크
  ↓
탄약 0이면         → Reload() 걸고 종료
  ↓
GetViewPoint()     카메라(플레이어) 또는 폰 시점(AI)
  ↓
FMath::VRandCone() AimSpreadHalfAngle 만큼 탄 퍼짐
  ↓
LineTraceSingleByChannel(ECC_Visibility)
  ↓
진영 확인          같은 편이면 "holding fire" 표시하고 데미지 없음
  ↓
ApplyPointDamage() → AActor::TakeDamage → OnTakeAnyDamage → UHealthComponent
  ↓
ApplyRecoil()      컨트롤러 회전에 가산, 나중에 같은 양만큼 되돌림
  ↓
ReportNoiseEvent() AI 청각에 총성 전달
  ↓
SetTimer()         FireRate 기반 다음 발사
```

**볼 만한 곳**
- `GetViewPoint()` — 플레이어는 카메라, AI는 폰 시점. 같은 컴포넌트를 양쪽이 쓰는 핵심
- `ApplyRecoil()` / `RecoverRecoil()` — 반동을 컨트롤러 회전에 더하고, 미발사 시 같은 양을 빼서 복구
- `GetWeaponData()` — 에셋이 없으면 런타임 기본값 생성. 에셋 없이도 동작하는 이유

---

## 3. AEnemyAIController — 전투 상태머신

### 구조

```cpp
Tick()
 ├─ UpdateSenses()             시야 유지 판정
 ├─ RetryFailedMove()          경로 실패 재시도
 ├─ UpdateGlobalTransitions()  상태 무관 규칙        ← virtual
 ├─ UpdateState(CurrentState)  현재 상태 Update      ← virtual
 └─ DrawStateDebug()           머리 위 텍스트

SetState(New)
 ├─ ExitState(Current)   ← virtual
 ├─ CurrentState = New
 └─ EnterState(New)      ← virtual
```

`Enter/Update/ExitState`가 **virtual + switch**입니다. 아군 컨트롤러가 이걸 오버라이드해서
자기 명령만 처리하고 나머지는 `Super::`로 넘깁니다. 새 상태를 얹는 유일한 접점입니다.

### 읽는 법

`Idle → Investigate → Engage → Cover → Suppress` 순으로 6쌍을 읽으면 끝입니다.
각 상태는 30줄을 안 넘습니다.

### 주의해서 볼 4곳

**(1) `UpdateGlobalTransitions()` — Flank 예외**
```cpp
if (IsInCombat() && CurrentState != ECQBAIState::Flank)
```
우회는 시야를 일부러 끊는 기동이라 "3초 시야 상실 → Investigate"에서 뺐습니다.
안 빼면 측면 복도(편도 22m, 6초) 중간에 모든 flank가 취소됩니다.

**(2) `HasReachedGoal()` + `RetryFailedMove()`**
경로 요청 실패를 "도착"으로 처리하면 적이 그냥 포기합니다. NavMesh 생성 중에 실제로 그랬습니다.
재시도 예산(`MaxMoveRetries`)을 다 쓸 때까지 도착으로 치지 않습니다.

**(3) `FindCoverPointFallback()`**
EQS 에셋 없이 동작하는 엄폐 탐색. NavMesh 24지점 샘플링 → 플레이어 눈높이에서
후보 지점으로 트레이스가 **막히는** 곳만 통과 → 자기 위치에서 가장 가까운 곳.

**(4) `IsHostile()`**
`FCQBFactions::AreHostile()`로 판단합니다. 예전엔 "플레이어인가"를 하드코딩했는데,
아군이 생기면서 진영 비교로 바꿨습니다. 죽은 대상은 위협이 아니므로 제외합니다.

---

## 4. AAllyAIController — 명령받는 분대원

`AEnemyAIController`를 **상속**합니다. 인지·Engage·Cover·Suppress·사격이 전부 따라옵니다.
아군이 하는 일은 진영을 바꾸고, 적 분대 매니저 등록을 건너뛰고, 명령 4개를 얹는 것뿐입니다.

| 명령 | 키 | 동작 |
|---|---|---|
| `Follow` | Z | 플레이어 뒤 300 유지. 플레이어가 150 이상 움직여야 재경로 |
| `Hold` | H | 제자리 정지, 적 보이면 교전 |
| `Stack` | 문 조준 + 1 | 배정된 좌/우 스택 지점으로, 도착 시 "In position" |
| `Clear` | 문 조준 + 2 | 문 너머 방 중심으로 진입, 3초간 적 없으면 "Room clear!" → Follow |
| `Watch` | 지점 조준 + 3 | 제자리에서 그 지점을 계속 주시 |

**분대는 4명, Red 2 / Blue 2입니다.** 마우스 휠로 지휘 대상을 RED → BLUE → GOLD(전체)로
순환합니다. 명령은 선택된 element에만 갑니다 — Blue가 문에 스택하는 동안 Red는 복도를 지키는 식.
`AAllyAIController::OnPossess()`가 짝/홀로 element를 배정하고 `RED_1` ~ `BLU_2`로 이름을 붙입니다.

명령이 떨어지면 **바닥에 element 색 링**이 4초간 그려집니다 (`DrawOrderMarker`).

### 명령과 전투의 관계

```
명령 수행 중 적 발견 → Engage (부모 상태머신)
                          ↓ 교전 종료(시야 3초 상실)
                    StandingOrder 복귀
```

`StandingOrder`에 마지막 명령을 기억해뒀다가 교전이 끝나면 돌아갑니다.
**`Clear`만 예외** — 방에 들어가는 게 명령 자체라 상태를 안 떠나고 그 자리에서 싸웁니다.

### `UpdateFollow()`의 NavMesh 투영

"플레이어 뒤 3m"는 실내에서 자주 벽 속입니다(플레이어가 벽을 등지면 항상).
그래서 투영하고, 실패하면 플레이어 위치로 붙습니다.

---

## 5. 항복·제압 — 이 샘플이 슈터가 아닌 이유

플레이어가 용의자를 조준하고 **F**를 누르면 "Drop the weapon!"을 외칩니다.
AI가 그 자리에서 판단합니다 — `AEnemyAIController::ReceiveChallenge()`.

### 순응 압력 계산 `EvaluateCompliance()`

| 요인 | 가중치 | 근거 |
|---|---|---|
| 부상 정도 | 최대 1.0 | 체력이 60% 아래로 떨어진 만큼 비례 |
| 외침 거리 | 최대 0.5 | 1200 이내에서 가까울수록 |
| 고립 | +0.4 | 같은 편이 전부 죽거나 항복했으면 |
| 시야 없음 | -0.3 | 상대가 안 보이면 덜 위협적 |
| 조준당함 | +0.35 | 플레이어가 직접 겨누고 외칠 때 |

합계가 `ComplianceThreshold`(1.0)를 넘으면 항복합니다.

### 실측 (헤드리스 3회)

```
만체력 / 동료 생존 / 1689 거리   →  0.35 / 1.00   "Not a chance!"
만체력 / 고립 / 675 거리         →  0.97 / 1.00   거부 (간발의 차)
30% 체력 / 고립 / 307 거리       →  1.22 / 1.00   "Hands up, I give up!"
```

**동전 던지기가 아니라 상황을 읽습니다.** 0.97로 버티는 케이스가 이 시스템의 값어치입니다.

### 항복 후

`Surrender` 상태는 **되돌아오지 않습니다** (`UpdateGlobalTransitions` 첫 줄에서 차단).
무기를 숨기고 무릎 높이로 내려앉으며, `IsHostile()`이 false를 반환해
**양 진영 AI가 모두 사격을 멈춥니다.** 손 든 사람을 분대원이 쏘지 않는 이유입니다.
분대 역할을 들고 있었다면 반납해서 남은 적들이 재배정받습니다.

---

## 6. ADoorwayMarker — 명령의 대상

액터의 **forward 벡터가 진입 방향**입니다. 에디터에서 회전만 시키면 방향이 정해집니다.

```
GetStackPoint(Left/Right)   문 옆 55cm, 뒤로 110cm
GetClearPoint()             문 너머 450cm
```

**좌우 오프셋이 55cm인 이유:** 복도 폭이 2m라 100cm를 쓰면 정확히 벽입니다.

**콜리전이 없는 이유 (중요):** 처음엔 조준용 박스를 달았는데, 그 박스가 `ECC_Visibility`를
막았습니다. **AI Sight가 같은 채널을 씁니다.** 문에 마커를 놓으면 그 문을 지나는
모든 AI 시야가 차단됐습니다. 지금은 `AProjectTFCharacter::UpdateAimedDoorway()`가
크로스헤어 각도로 고르고, 벽 뒤인지만 트레이스 1회로 확인합니다.

---

## 7. ASquadManager — 적 분대

세 함수만 보면 됩니다. **적 전용입니다** — 아군은 플레이어가 직접 지휘하므로 등록하지 않습니다
(`ShouldJoinSquad()`가 아군에서 false).

| 함수 | 하는 일 |
|---|---|
| `RequestRole()` | 역할 가진 인원 수 기준. 짝수번째 Suppressor, 홀수번째 Flanker(좌우 교대) |
| `GetFlankPoint()` | 플레이어→적 벡터 ±90도, 거리 600, NavMesh 투영. 실패 시 반대쪽 → 랜덤 |
| `NotifyEnemyDied()` | "Man down!" → 전원 역할 초기화 → 생존자 재요청 |

`ReassignRoles()`가 **2패스**인 이유: 1패스면 먼저 재요청한 적이 아직 옛 역할을 든
다른 적들을 세어버립니다. 전부 비운 뒤 재요청해야 맞습니다.

---

## 8. 수정한 템플릿 파일 3개

| 파일 | 무엇을 |
|---|---|
| `ProjectTFCharacter.h/.cpp` | Health/Weapon/WeaponVisual/NavInvoker 컴포넌트, Fire·ADS·Reload·Lean 입력, 분대 명령 입력, 진영·시야타깃 인터페이스 |
| `ProjectTFCameraManager.cpp` | `UpdateViewTarget()`에서 린(roll + 측면 오프셋)을 최종 POV에 적용 |
| `ProjectTFPlayerController.cpp` | `BeginPlay`에서 `ClientSetHUD(ACQBHUD)`, 팀 ID |

`Variant_Shooter` / `Variant_Horror`는 **최초 상태 그대로**입니다 (git diff로 확인 가능).

**린을 왜 카메라 매니저에서 처리하나:** 카메라가 `bUsePawnControlRotation=true`라
컴포넌트에 roll을 넣어도 매 프레임 덮어씁니다. 최종 POV 결정 후에 더해야 합니다.

---

## 9. 설계 선택과 이유

| 선택 | 이유 |
|---|---|
| Behavior Tree 대신 직접 만든 상태머신 | 요구사항. 전이 규칙이 코드에 다 보이고 로그로 추적됨 |
| 아군이 적 컨트롤러를 **상속** | 전투 로직 한 벌만 유지. 아군은 진영 + 명령 4개만 추가 |
| 플레이어와 AI가 같은 컴포넌트 | 체력·무기 로직 한 벌. AI는 `StartFire()`를 호출할 뿐 |
| 데미지를 `ApplyPointDamage` 경유 | 직접 호출하면 God 치트·데미지 타입·면역이 무시됨 |
| 무기를 카메라에 부착 | 손에 붙이면 시야 회전을 안 따라옴(팔 애님이 조준 데이터를 요구) |
| 입력을 `BindKey` 직접 바인딩 | IA 에셋 없이 동작. IA를 지정하면 그쪽이 우선 |

---

## 10. 함정 모음 — 실제로 며칠 날린 것들

읽는 사람이 같은 데 빠지지 않도록 적어둡니다.

**AI Sight는 액터 원점(허리)으로 트레이스합니다.**
엄폐물이 110~120cm면, 머리와 가슴이 훤히 보이는 사람이 "완전히 숨은" 것으로 판정됩니다.
`LineOfSightTo()`는 눈높이라 "보인다"고 답해서 둘이 어긋납니다.
→ `CQBSightTarget` + `IAISightTargetInterface`로 **눈·가슴·허리 3점**을 테스트합니다.

**`bGenerateNavigationOnlyAroundNavigationInvokers=True`면 `Build()`가 타일을 0개 만듭니다.**
볼륨도 지오메트리도 정상인데 NavMesh가 통째로 안 생깁니다. 지금은 False입니다.

**커맨드릿은 네비게이션 빌드를 잠급니다** (`navigation build is locked`).
헤드리스로는 네비를 구울 수 없습니다.

**생성자에서 블루프린트를 로드하면 async 로더가 데드락합니다**
(`Loading is stuck, flush will never finish`). 전부 `TSoftObjectPtr` + `BeginPlay` 로드입니다.

**`SetVisibility(false, true)`는 자식까지 숨깁니다.** 카메라의 자식인 무기까지 사라집니다.

**레벨 스크립트는 맵을 통째로 덮어씁니다.** 손으로 다듬기 시작했으면 다시 돌리면 안 됩니다.

---

## 11. 직접 돌려볼 때

```bash
# AI 전체 시퀀스 (로그만)
ProjectTF.exe -game -nullrhi -unattended -stdout

# 아군 사망 → 역할 재배정
ProjectTF.exe ... -CQBKillEnemyAfter=10

# 분대 명령 (0=방A 문, 1=방B 문). stack / clear / watch / hold / follow / challenge
ProjectTF.exe ... -CQBOrder=stack -CQBOrderAfter=8 -CQBOrderDoor=1
ProjectTF.exe ... -CQBOrder=clear -CQBOrderAfter=8 -CQBOrderDoor=0

# 항복 검증: 3명을 70% 깎고 9초 뒤 가장 가까운 용의자에게 외침
ProjectTF.exe ... -CQBKillEnemyAfter=5 -CQBKillCount=3 -CQBKillDamage=0.7                   -CQBOrder=challenge -CQBOrderAfter=9

# 스크린샷
ProjectTF.exe -game -windowed -CQBScreenshotAfter=5

# 인지 디버깅이 필요하면
log LogProjectTF Verbose
```

로그에서 `CQB` 로 시작하는 줄만 보면 전부 추적됩니다.

---

## 12. 검증된 시퀀스 (헤드리스 로그)

**적 분대 교전**
```
[Enemy_1] Contact!                     시야 획득 + 분대 전파
Enemy_2, Enemy_3  Idle -> Investigate  콜아웃 듣고 이동
Enemy_1  Engage -> Cover -> Suppress   1번째 = Suppressor
Enemy_2  Engage -> Flank               2번째 = Flanker
[Enemy_2] Flanking left!
Enemy_2  Flank -> Engage               측면 지점 도착
Enemy_3  Engage -> Cover -> Suppress   3번째 = Suppressor
```

**항복**
```
CQB debug: challenging EnemyCharacter_0 at 307
Enemy_1  Suppress -> Surrender
[Enemy_1] Hands up, I give up!     surrendered (1.22 of 1.00)
```

**아군 명령**
```
scripted order 'stack' on Room B
RED_1/2, BLU_1/2  Follow -> Stack   →  전원 In position

scripted order 'clear' on Room A
Ally_1/2  Follow -> Clear   →  [Ally_1] Room clear!  →  Clear -> Follow

scripted order 'watch' on Room B
RED_1/2, BLU_1/2  Follow -> Watch   →  전원 "Watching that"
```

**시야 상실 / 아군 사망**
```
[Enemy_2] Lost visual      Enemy_2  Suppress -> Investigate
[Enemy_1] Man down!        Enemy_2  Suppress -> Engage (역할 재요청)
```

---

## 13. 구조 정리 — 무엇이 어디에 있나

```
┌─ Config (.ini)          프로젝트 전역. 재시작 필요
├─ Level (Lvl_CQB)        배치. 에디터에서 즉시
├─ Data Asset             수치 묶음. 재컴파일 없음
└─ C++                    규칙·상태머신 — 여기만 진짜 "로직"
```

**원칙:** "적이 3초 뒤 포기한다"는 규칙은 C++, "3초"라는 값은 데이터.

### 바꾸려면 어디로

| 바꿀 것 | 어디서 | 재컴파일 |
|---|---|---|
| 무기 수치 | `UWeaponData` 에셋 → 캐릭터 BP의 Weapon Component | 없음 |
| 적 체력·무기 | `BP_Enemy` (AEnemyCharacter 상속) | 없음 |
| AI 인지·전투 타이밍 | `BP_EnemyAIController` | 없음 |
| 아군 명령 거리·시간 | `BP_AllyAIController` (AAllyAIController 상속) | 없음 |
| 분대 측면거리·콜아웃 | 레벨에 배치한 `SquadManager` | 없음 |
| 문 위치·스택 지점 | 레벨의 `DoorwayMarker` (`bDrawDebug`로 확인) | 없음 |
| 적/아군 수·위치 | 레벨의 `EnemySpawner` / `AllySpawner` | 없음 |
| 레벨 형태 | `Scripts/BuildCQBLevel.py` | 없음 (레벨 재생성) |
| 전이 규칙·역할 배분 | `CQB/*.cpp` | **필요** |

### BP 껍데기 만드는 법

적/아군 AI 값을 만지려면 한 번만 만들어두면 됩니다:

```
Blueprint Class → AEnemyAIController  → BP_EnemyAIController
Blueprint Class → AEnemyCharacter     → BP_Enemy
  Class Defaults → AI Controller Class = BP_EnemyAIController
레벨의 EnemySpawner → Enemy Class = BP_Enemy
```

아군도 같은 방식 (`AAllyAIController` / `AAllyCharacter`).

### 알아둘 것 2개

- **`SquadManager`는 없으면 런타임 자동 생성**됩니다. 값을 만지려면 레벨에 직접 배치해야 합니다.
- **`AllySpawner`는 `AEnemySpawner` 클래스를 재사용**합니다. 프로퍼티 이름이 `EnemyClass`인 건 그 때문입니다.

---

## 14. 아직 안 열어둔 것

- **BP 확장 훅 없음** (`BlueprintImplementableEvent` 0개) — 사운드·머즐 플래시를 BP에서 붙일 자리가 없습니다.
  붙인다면 `OnStateChanged` / `OnCalloutSpoken` / `OnWeaponFired` / `OnEnemyDied`.
- **적 프로파일 DataAsset 없음** — AI 수치 12개가 컨트롤러에 흩어져 있어 성격을 바꾸려면 BP 복제가 필요합니다.
- **`EnemyAIController.cpp`가 888줄** — 함수는 다 짧지만, 1,200줄을 넘기면 전투/인지/이동으로 나누는 게 맞습니다.
- **`AEnemySpawner`가 일을 3개** — 스폰 + 네비 확인 + 디버그 훅.

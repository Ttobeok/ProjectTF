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

---

## 7. 구조 정리 — 무엇이 어디에 있나

4개 층으로 나뉩니다. 위로 갈수록 자주 바뀌고, 바꾸는 비용이 쌉니다.

```
┌─ Config (.ini)          프로젝트 전역. 재시작 필요
│   DefaultEngine.ini        기본 맵, NavMesh 인보커 설정
│   DefaultGame.ini          패키징 설정
│
├─ Level (Lvl_CQB)         배치. 에디터에서 즉시
│   EnemySpawner             적 수·위치, 스폰할 적 클래스
│   PlayerStart / 조명 / 지오메트리
│   (SquadManager)           없으면 런타임 자동 생성
│
├─ Data Asset              수치 묶음. 재컴파일 없음
│   UWeaponData              무기 한 벌의 모든 수치
│
└─ C++ (Source/ProjectTF)  규칙·상태머신·시스템
    CQB/*.h 의 UPROPERTY     기본값. BP로 덮어쓸 수 있음
    CQB/*.cpp                전이 조건, 배분 규칙 — 여기만 진짜 "로직"
```

**핵심 원칙:** C++는 *규칙*, 그 위는 전부 *수치와 배치*입니다.
"적이 3초 뒤 포기한다"는 규칙은 C++, "3초"라는 값은 데이터입니다.

---

## 8. 데이터 수정 — 무엇을 바꾸려면 어디로

### 한눈에

| 바꾸고 싶은 것 | 어디서 | 재컴파일 | 적용 범위 |
|---|---|---|---|
| 무기 수치 (데미지·연사·반동·FOV) | `UWeaponData` 에셋 | 없음 | 그 에셋을 쓰는 무기 전부 |
| 적 체력·무기·래그돌 | `BP_Enemy` (AEnemyCharacter 상속) | 없음 | 그 BP로 스폰된 적 |
| AI 인지·전투 타이밍 | `BP_EnemyAIController` | 없음 | 그 컨트롤러를 쓰는 적 |
| 분대 측면거리·콜아웃 시간 | 레벨에 배치한 `SquadManager` | 없음 | 그 레벨 |
| 적 수·배치 위치 | 레벨의 `EnemySpawner` | 없음 | 그 레벨 |
| 레벨 형태 (방 크기·복도·엄폐물) | `Scripts/BuildCQBLevel.py` | 없음 | 레벨 재생성 |
| 전이 규칙·역할 배분 로직 | `CQB/*.cpp` | **필요** | 전역 |
| 기본값 자체 | `CQB/*.h` | **필요** | 전역 |

### 1) 무기 수치 — Data Asset

```
Content Browser 우클릭 → Miscellaneous → Data Asset → WeaponData 선택
  → DA_Rifle_Player 생성, 값 입력
  → BP_FirstPersonCharacter 열기 → Weapon Component 선택
  → Weapon Data = DA_Rifle_Player
```

에셋을 안 물리면 `UWeaponComponent::GetWeaponData()`가 런타임 기본값을 만들어 씁니다.
그래서 에셋 없이도 동작하고, 물리면 그쪽이 이깁니다.

적 무기는 `AEnemyCharacter::BeginPlay()`가 `EnemyWeaponDamage` 등으로 런타임 생성하는데,
BP에서 `Weapon Data`를 물리면 그게 우선입니다.

| 필드 | 기본 | 의미 |
|---|---|---|
| `Damage` | 20 | 1발 데미지 |
| `FireRate` | 8 | 초당 발사 수. 간격 = 1/FireRate |
| `MagSize` / `ReloadTime` | 30 / 1.8s | 탄창 / 재장전 |
| `RecoilPitchMin/Max` | 0.25~0.7 | 발당 상승 각도 |
| `RecoilYawMin/Max` | ±0.25 | 발당 좌우 흔들림 |
| `RecoilRecoverySpeed` | 6 °/s | 미발사 시 복구 속도 |
| `ADSFov` / `HipFov` | 55 / 90 | 조준 / 평상시 시야각 |
| `NoiseLoudness` / `NoiseRange` | 2 / 4000 | AI 청각에 전달되는 총성 |

### 2) 적 — BP 껍데기 2개

지금은 적이 C++ 클래스로 직접 스폰돼 만질 디테일 패널이 없습니다. 한 번만 만들어두면 됩니다:

```
1) Blueprint Class → AEnemyAIController 검색 → BP_EnemyAIController
     AI|Perception   SightRadius, SightAngle, HearingRange
     AI|Combat       LoseSightGraceTime, InvestigateGiveUpTime, EngageMinDuration,
                     SuppressFireDuration, SuppressRestDuration,
                     CoverSearchRadius, MinCoverDistanceFromPlayer,
                     MoveRetryInterval, MaxMoveRetries, CoverQuery
     AI|Debug        bDrawStateDebug

2) Blueprint Class → AEnemyCharacter 검색 → BP_Enemy
     Class Defaults → AI Controller Class = BP_EnemyAIController
     Enemy          EnemyMaxHealth, bRagdollOnDeath, DeferredDestructionTime
     Enemy|Weapon   EnemyWeaponDamage, EnemyWeaponFireRate, EnemyAimSpreadHalfAngle

3) 레벨의 EnemySpawner 선택 → Enemy Class = BP_Enemy
```

적 성격을 여러 개 만들려면 BP_Enemy를 복제해서 값만 바꾸고, 스포너를 여러 개 두면 됩니다.

### 3) 분대 — 레벨에 직접 배치해야 함

`ASquadManager`는 월드에 없으면 **런타임에 자동 생성**됩니다. 편하지만, 그러면 값을 못 만집니다.
`FlankDistance`나 `CalloutDisplayTime`을 바꾸려면:

```
Place Actors에서 SquadManager 검색 → 레벨에 배치 → 디테일 패널에서 조정
```

레벨에 하나라도 있으면 `GetSquadManager()`가 그걸 씁니다 (자동 생성 안 함).

### 4) 적 배치 — EnemySpawner

```
Spawner
  EnemyClass         스폰할 적 클래스 (기본 AEnemyCharacter)
  SpawnOffsets       스포너 기준 상대 좌표 배열. 항목 수 = 적 수
  bSpawnOnBeginPlay  체크 해제하면 BP에서 SpawnEnemies() 직접 호출
```

각 지점은 NavMesh에 투영된 뒤 캡슐 높이만큼 올려 스폰합니다.

### 5) 레벨 형태 — Python 스크립트

방 크기·복도 폭·엄폐물 위치는 `Scripts/BuildCQBLevel.py` 상단 상수와
`build_geometry()` / `build_cover()` 좌표입니다.

```bash
UnrealEditor-Cmd.exe ProjectTF.uproject -run=pythonscript -script="Scripts/BuildCQBLevel.py"
```

레벨을 통째로 다시 만듭니다. **에디터에서 손으로 고친 건 날아갑니다.**
손으로 다듬기 시작했으면 스크립트는 그만 돌리거나, 변경분을 스크립트에 반영하세요.

### 6) 프로젝트 설정 — ini

| 파일 | 무엇 |
|---|---|
| `DefaultEngine.ini` | 시작 맵, `bGenerateNavigationOnlyAroundNavigationInvokers`, RecastNavMesh 설정 |
| `DefaultGame.ini` | 패키징 (쿡할 맵, pak 압축) |

---

## 9. 아직 안 열어둔 것

솔직히 적어둡니다. **BP 확장 훅이 없습니다** (`BlueprintImplementableEvent` 0개).
수치는 전부 뺐지만, 연출을 붙일 자리는 안 만들어뒀습니다.

붙이면 좋을 지점:

| 훅 | 쓸 곳 |
|---|---|
| `OnStateChanged(Old, New)` | 상태별 사운드·애님 몽타주 |
| `OnCalloutSpoken(Callout)` | 콜아웃 음성 |
| `OnWeaponFired()` | 머즐 플래시·탄피·사운드 |
| `OnEnemyDied()` | 사망 연출 |

그리고 **적 프로파일 DataAsset**도 아직 없습니다. 지금은 AI 수치 12개가
`AEnemyAIController`에 흩어져 있어서, 적 성격을 바꾸려면 BP를 복제해야 합니다.
`UEnemyProfileData` 하나로 묶으면 에셋만 갈아끼우면 됩니다.

둘 다 로직 변경이 아니라 **표면을 넓히는 작업**이라, 필요해질 때 해도 늦지 않습니다.

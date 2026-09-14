# ProjectTF — CQB Sample

UE 5.8 C++ First Person 템플릿 기반의 근접전(CQB) 샘플.
플레이어 사격/ADS/재장전/린, 적 AI 상태머신, 분대 단위 역할 배분과 콜아웃까지 **모든 로직이 C++**로 구현되어 있습니다.
Behavior Tree / StateTree는 사용하지 않고, 직접 작성한 상태머신이 AI를 구동합니다.

---

## 1. 실행 방법

### 에디터에서 실행

1. `ProjectTF.uproject` 실행 (UE 5.8)
2. `/Game/CQB/Lvl_CQB` 열기 (프로젝트 기본 맵으로 설정되어 있어 그냥 열면 됩니다)
3. Play

배치나 블루프린트 작업이 필요 없습니다.
- 레벨에 `EnemySpawner`, `PlayerStart`, `NavMeshBoundsVolume`, 조명이 모두 들어 있음
- `AEnemyCharacter`가 템플릿 마네킹 메시(SKM_Quinn_Simple)와 `ABP_Unarmed`를 생성자에서 직접 로드
- `ASquadManager`는 월드에 없으면 최초 요청 시 자동 스폰
- `UWeaponComponent`는 `WeaponData` 에셋이 비어 있으면 런타임 기본값 사용

> 레벨에 네비게이션 데이터가 저장되어 있지 않아 로드 시 런타임 빌드(약 0.3초)가 일어나고
> `SpawnMissingNavigationData` 경고가 뜹니다. 에디터에서 **Build > Build Paths** 후 레벨을 저장하면 사라집니다.

### 레벨 구조

**`/Game/CQB/Lvl_CQB`** — 엔진 기본 큐브(`/Engine/BasicShapes/Cube`)로 그레이박싱했고,
`Scripts/BuildCQBLevel.py`가 전부 생성합니다. CQB 스케일(방 8m, 복도 2m, 엄폐물 110~120cm)입니다.

```
 Y
 ↑                    ┌───────────────┐         ┌───────────────┐
 400                  │               ╳── 문 ──╳               │
                      │   ROOM A      │  2m    │   ROOM B      │
                      │   8m x 8m     │        │  [B1]①  [B2]② │
   0                  │      [A1]     │        │               │
       ┌────┐╳╳╳╳╳╳╳╳╳│               │        │     [B3]③     │
-250   │스폰│  진입    │          [A2] │        │               │
       └────┘ 복도 2m  │               │        │               │
-400                  └───╳╳╳╳────────┘         └───────╳╳╳╳────┘
-500                      │                             │
                          └─────────────────────────────┘
-700                          측면 복도 (AI 우회 경로) 2m

      -1400  -1000  -800            0   200            1000      X
```

`╳` 표시가 개구부입니다. **진입 복도는 방 A의 남쪽(Y -350~-150), 방 B로 가는 문은 북쪽(Y 150~350)** 으로
어긋나게 뚫려 있습니다. 일직선으로 뚫으면 스폰 지점에서 방 B가 보여 적이 바로 사격을 시작하고,
이 레벨이 노리는 "모서리 돌며 확인(pie-slicing)" 구간이 사라집니다.

| 요소 | 값 |
|---|---|
| 방 A / 방 B | 각 800 x 800 (8m x 8m), X[-800,0] / X[200,1000], Y[-400,400] |
| 벽 높이 / 두께 | 320 (3.2m) / 40 |
| 개구부 폭 | 200 (2m) — 진입 Y[-350,-150], 문 Y[150,350] |
| 엄폐물 | 높이 110~120 큐브 5개 (쭈그리면 가려지고, 서면 넘겨 쏠 수 있는 높이) |
| PlayerStart | (-1200, -250, 100), +X 방향 |
| EnemySpawner | (600, 0, 20) — 적 3명이 방 B의 엄폐물 옆에 전개 |
| 측면 복도 | 방 B 남쪽 출구 → Y[-700,-500] 통로 → 방 A 남서쪽 입구 |
| NavMeshBounds | 중심 (-200, -150, 140), 범위 2800 x 1600 x 800 |

**측면 복도가 이 레벨의 핵심입니다.** 우회로가 없으면 `Flank` 상태가 갈 곳이 없어서
AI가 결국 정면으로 밀고 들어옵니다. 이 복도 덕분에 Flanker 역할을 받은 적이
방 B 남쪽 → 서쪽 통로 → 방 A 남서쪽으로 돌아 들어와 **플레이어 뒤/옆에서** 나타납니다.

### 레벨을 다시 생성하려면

```bash
UnrealEditor-Cmd.exe ProjectTF.uproject -run=pythonscript -script="Scripts/BuildCQBLevel.py"
```

에디터 안에서는 **Tools > Execute Python Script**로도 실행할 수 있습니다.
치수는 스크립트 상단 상수와 `build_geometry()`의 좌표만 고치면 됩니다.

### 조작

| 입력 | 동작 |
|---|---|
| 좌클릭 | 사격 (자동, FireRate 기준) |
| 우클릭 | ADS (카메라 FOV 보간) |
| R | 재장전 |
| Q / E | 좌/우 린 (카메라 roll + 좌우 오프셋) |
| WASD / Space / 마우스 | 템플릿 기본 이동·점프·시점 |

Fire/ADS/Reload/Lean은 **Input Action 에셋 없이도** 동작합니다 (`UInputComponent::BindKey` 직접 바인딩).
Enhanced Input으로 바꾸고 싶으면 캐릭터 BP의 `Input|CQB` 카테고리에 있는
`CQBFireAction` / `CQBADSAction` / `CQBReloadAction` / `CQBLeanLeftAction` / `CQBLeanRightAction`에
IA 에셋을 지정하면 해당 입력만 Enhanced Input 경로로 전환됩니다.

### 빌드

```bash
# 에디터 타깃 (에디터를 먼저 종료해야 함 — Live Coding이 잡고 있으면 UBT가 거부)
Build.bat ProjectTFEditor Win64 Development -Project="...\ProjectTF.uproject"

# 게임 타깃
Build.bat ProjectTF Win64 Development -Project="...\ProjectTF.uproject"
```

에디터를 켠 채로 코드를 고쳤다면 커맨드라인 대신 에디터에서 **Ctrl+Alt+F11**(Live Coding)을 쓰면 됩니다.

---

## 2. 클래스 구조

```
Source/ProjectTF/
├── ProjectTFCharacter          (ACharacter, abstract)  플레이어. BP_FirstPersonCharacter의 부모
│   ├── UHealthComponent                                체력
│   ├── UWeaponComponent                                히트스캔 무기
│   └── 입력: Fire / ADS / Reload / Lean(Q,E)
├── ProjectTFCameraManager      (APlayerCameraManager)  린(roll + 측면 오프셋)을 최종 POV에 적용
│
└── CQB/
    ├── CQBTypes.h                  EEnemyState / ESquadRole / ECalloutType + 문자열 헬퍼
    ├── UHealthComponent            MaxHealth, CurrentHealth, TakeDamage, OnDeath, OnHealthChanged
    │                               └ 엔진 데미지(OnTakeAnyDamage)도 이 컴포넌트로 연결
    ├── UWeaponData                 (UPrimaryDataAsset) Damage, FireRate, MagSize, ReloadTime,
    │                               Recoil Pitch/Yaw Min/Max, ADSFov, HipFov, Range, Noise...
    ├── UWeaponComponent            Fire / StartFire / StopFire / Reload / StartADS / StopADS
    │                               라인트레이스 → UHealthComponent에 데미지
    │                               반동(컨트롤러 회전 가산 + 자동 회복), FOV 보간,
    │                               발사 시 UAISense_Hearing 노이즈 이벤트
    ├── AEnemyCharacter             (ACharacter) 적. 플레이어와 같은 Health/Weapon 컴포넌트 재사용
    │                               사망 시 래그돌 → 6초 후 Destroy
    ├── AEnemyAIController          (AAIController) 상태머신 + AIPerception(Sight/Hearing)
    │                               EQS 또는 NavMesh 샘플링으로 엄폐 지점 탐색
    ├── ASquadManager               (AActor, 월드 1개) 역할 배분 / 측면 지점 / 콜아웃 중계
    ├── AEnemySpawner               (AActor) 적 N명 배치
    └── UEnvQueryContext_CQBPlayer  (UEnvQueryContext) EQS에서 플레이어를 가리키는 컨텍스트
```

### 데이터 흐름

```
[플레이어 입력] → AProjectTFCharacter → UWeaponComponent::Fire()
                                          ├→ LineTrace → UHealthComponent::TakeDamage()
                                          ├→ 컨트롤러 회전에 반동 가산
                                          └→ UAISense_Hearing::ReportNoiseEvent()
                                                            ↓
                        AEnemyAIController::OnPerceptionUpdated(Sight / Hearing)
                                                            ↓
                                            상태머신 Tick → UpdateState()
                                                            ↕
                                      ASquadManager (역할 / 측면 지점 / 콜아웃)
```

---

## 3. 적 AI 상태 전이표

상태별로 `Enter / Update / Exit` 함수를 가지며, `Tick`이 현재 상태의 `Update`를,
`SetState()`가 `Exit(old) → Enter(new)`를 호출합니다.

| 상태 | Enter | Update | 전이 조건 → 다음 상태 |
|---|---|---|---|
| **Idle** | 정지, 포커스 해제 | 대기 | 시각 자극 → `Engage`<br>청각(총성) 자극 → `Investigate`<br>아군 "Contact!" 수신 → `Investigate` |
| **Investigate** | 마지막 자극 위치로 이동 | 도착 판정 + 대기 시간 누적 | 플레이어 시야 확보 → `Engage`<br>도착 후 3초 미발견 → `Idle` |
| **Engage** | 플레이어 포커스, 사격 시작,<br>SquadManager에 역할 요청 | 최소 1.2초 사격 | 역할 = Flanker & 미수행 → `Flank`<br>그 외 → `Cover` |
| **Cover** | 사격 중지, 엄폐 지점 탐색(EQS 또는 NavMesh) 후 이동 | 도착 판정 | 도착 → `Suppress`<br>엄폐 지점 없음 → `Suppress` |
| **Suppress** | "Suppressing!" 콜아웃, 사격 시작 | 2초 사격 / 1초 휴식 반복 | (전역 규칙으로만 이탈) |
| **Flank** | 측면 지점으로 이동,<br>"Flanking left/right!" 콜아웃 | 도착 판정 | 도착 → `Engage` (이후 재플랭크 금지) |

**전역 전이 규칙**

| 조건 | 적용 상태 | 결과 |
|---|---|---|
| 플레이어 시야 3초 이상 상실 | `Engage` / `Cover` / `Suppress` | "Lost visual" 콜아웃 → `Investigate` (마지막 목격 위치) |
| 플레이어 사망 | 전체 | `Idle` |
| 시야 확보 | `Idle` / `Investigate` | `Engage` |

> **`Flank`는 시야 상실 규칙에서 제외했습니다.** 우회는 시야를 일부러 끊는 기동이고,
> 이 레벨의 측면 복도는 편도 약 22m(6초)라 규칙을 그대로 적용하면 모든 flank가 중간에 취소됩니다.
> 도착해서 `Engage`로 돌아온 뒤부터 다시 규칙이 적용됩니다.

시야 판정은 **Perception(Sight)으로 최초 획득**(반경 2000, 시야각 70도) 후,
매 프레임 `LineOfSightTo()`로 확인합니다. 엄폐물 뒤로 숨으면 즉시 반영됩니다.

---

## 4. 분대(SquadManager)

### 역할 배분 — `RequestRole()`

`Engage`에 진입한 적이 요청하며, 이미 역할을 가진 인원 수를 기준으로 배정합니다.

| 교전 진입 순서 | 역할 |
|---|---|
| 1번째 | `Suppressor` → Cover → Suppress |
| 2번째 | `Flanker` (좌/우 교대) → Flank → Engage |
| 3번째 | `Suppressor` |
| 4번째 | `Flanker` (반대쪽) |

적이 사망하면 `NotifyEnemyDied()` → 전원 역할 초기화 → 생존자가 다시 요청하여 **역할 재배정**됩니다.

### 측면 지점 — `GetFlankPoint()`

플레이어 → 적 방향 벡터를 플레이어 기준으로 **좌/우 90도 회전**, 거리 600에서 지점을 잡고
`ProjectPointToNavigation()`으로 NavMesh에 투영합니다.

배정된 쪽이 NavMesh 밖이면(실내 맵은 한쪽으로만 우회로가 있는 경우가 많음) **반대쪽을 시도**하고,
양쪽 다 실패하면 플레이어 주변의 도달 가능한 지점으로 대체합니다.

### 콜아웃 — `Broadcast()`

화면에 `[Enemy_1] Flanking left!` 형식으로 3초간 표시되고, 모든 적에게 전달됩니다.

| 트리거 | 문구 |
|---|---|
| 최초 발견 | `Contact!` |
| Suppress 진입 | `Suppressing!` |
| Flank 진입 | `Flanking left!` / `Flanking right!` |
| 시야 3초 상실 | `Lost visual` |
| 아군 사망 | `Man down!` |

`Contact!`를 수신한 Idle 상태의 적은 발신자가 마지막으로 본 플레이어 위치로 `Investigate`합니다.

---

## 5. 엄폐 지점 탐색 (EQS)

`AEnemyAIController::FindCoverPoint()`는 두 가지 경로를 지원합니다.

1. **EQS** — 컨트롤러의 `CoverQuery`에 EQS 에셋을 지정하면 C++에서 `FEnvQueryRequest::Execute()`로 실행.
   쿼리 작성 시 Context로 **`EnvQueryContext_CQBPlayer`**(플레이어 반환)를 쓰고,
   Generator는 `SimpleGrid`(Querier 기준 반경 800), Test는 `Trace`(Context = 플레이어, 실패한 지점만 통과)를 사용하면 됩니다.
2. **내장 NavMesh 탐색 (기본값)** — EQS 에셋 없이 동작.
   `GetRandomReachablePointInRadius()`로 반경 800 내 24지점을 샘플링하고,
   플레이어 눈높이 → 후보 지점(가슴 높이) 트레이스가 **막힌** 지점만 엄폐로 인정,
   플레이어와 350 이상 떨어진 후보 중 자기 위치에서 가장 가까운 지점을 선택합니다.

즉 EQS 에셋을 만들지 않아도 엄폐 행동이 동작하며, 만들면 그쪽이 우선합니다.

---

## 6. 디버그 표시

| 위치 | 내용 |
|---|---|
| 화면 좌상단 | 발사 `[Player] FIRE 27/30`, 명중 `[Player] HIT Enemy_1 -20 (40 HP left)`, 재장전 `RELOADING... / RELOADED` |
| 화면 좌상단 | 피격 `TAKING FIRE from Enemy_2  HP 79/100`, 사망 `YOU ARE DOWN` |
| 화면 좌상단 | 콜아웃 `[Enemy_1] Flanking left!` (3초) |
| 적 머리 위 | `Enemy_1  [Engage]  Suppressor  LOS` |
| 월드 | 탄도 라인(노랑), 명중 지점(빨강 구), 벽 명중(흰 점) |
| 로그 (`LogProjectTF`) | `CQB: spawned 3 enemies`, `CQB: Enemy_1  Engage -> Cover`, `CQB callout: [Enemy_1] Suppressing!` |

로그만으로도 AI 동작을 추적할 수 있어서, 화면 없이 돌려도(`-game -nullrhi`) 상태 전이를 확인할 수 있습니다.

적 무기는 `bShowDebugMessages = false`라 화면 텍스트를 오염시키지 않습니다.

---

## 7. 주요 파라미터

| 항목 | 기본값 | 위치 |
|---|---|---|
| 플레이어 체력 | 100 | `UHealthComponent::MaxHealth` |
| 플레이어 무기 | Damage 20 / FireRate 8 / Mag 30 / Reload 1.8s | `UWeaponData` 기본값 |
| ADS FOV / Hip FOV | 55 / 90 | `UWeaponData` |
| 반동 | Pitch 0.25~0.7, Yaw -0.25~0.25, 회복 6°/s | `UWeaponData` |
| 적 체력 | 60 | `AEnemyCharacter::EnemyMaxHealth` |
| 적 무기 | Damage 7 / FireRate 2.5 / 탄퍼짐 4° | `AEnemyCharacter::Enemy*` |
| 시야 | 반경 2000 / 각도 70 | `AEnemyAIController::SightRadius, SightAngle` |
| 청각 | 3000 | `AEnemyAIController::HearingRange` |
| 시야 상실 유예 | 3초 | `LoseSightGraceTime` |
| 제압 사격 주기 | 2초 사격 / 1초 휴식 | `SuppressFireDuration / SuppressRestDuration` |
| 측면 거리 | 600 | `ASquadManager::FlankDistance` |

모두 `EditDefaultsOnly / EditAnywhere`라 BP나 디테일 패널에서 조정 가능합니다.

---

## 8. 참고

- 템플릿의 `Variant_Shooter`(발사체 기반 무기 + StateTree AI)와 `Variant_Horror`는 **그대로 남겨두었습니다.**
  CQB 구현은 기본 First Person 경로(`Lvl_FirstPerson` / `BP_FirstPersonCharacter`)에 붙어 있으며,
  새 사격은 발사체가 아닌 **라인트레이스**입니다.
- `AProjectTFCharacter`는 `abstract`이므로 레벨에는 `BP_FirstPersonCharacter`가 배치되어야 합니다
  (템플릿 기본 상태 그대로면 이미 그렇습니다).
- 적 사격이 플레이어에게 닿도록 플레이어/적 캡슐의 `ECC_Visibility` 응답을 `Block`으로 설정했습니다.

## 9. 패키징

```
Platforms → Windows → Shipping → Package Project
```

산출물: `Windows/ProjectTF.exe`

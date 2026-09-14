# ProjectTF — CQB Sample

UE 5.8 C++ First Person 템플릿 기반 근접전(CQB) 샘플.
플레이어 사격/ADS/재장전/린, 적 AI 상태머신, 분대 역할 배분과 콜아웃까지 **모든 게임 로직이 C++** 입니다.
Behavior Tree / StateTree는 쓰지 않고, 직접 작성한 상태머신이 AI를 구동합니다.

**실행 파일:** _(드라이브 링크 — 업로드 후 기입)_

---

## 1. 실행 방법

### 패키지 실행

압축 해제 후 `Windows/ProjectTF.exe` 실행. 설치 불필요.

> Shipping이 아니라 **Development** 빌드입니다. Shipping은 `DrawDebug*`와 화면 디버그 텍스트가
> 컴파일 단계에서 제거돼서, 이 샘플의 핵심인 적 상태 표시와 콜아웃이 전혀 보이지 않습니다.

### 에디터 실행

1. `ProjectTF.uproject` 실행 (UE 5.8)
2. `/Game/CQB/Lvl_CQB` — 프로젝트 기본 맵이라 그냥 Play

배치·블루프린트 작업이 필요 없습니다. 레벨에 스포너/PlayerStart/NavMesh 볼륨/조명이 모두 들어 있고,
적 메시·애님·무기는 C++ 생성자에서 로드하며, `ASquadManager`는 없으면 자동 스폰됩니다.

### 조작

| 입력 | 동작 |
|---|---|
| WASD / 마우스 | 이동 / 시점 |
| 좌클릭 | 사격 (FireRate 기반 연사) |
| 우클릭 | ADS — 카메라 FOV 보간 + 조준선 수축 |
| R | 재장전 |
| Q / E | 좌/우 린 (카메라 roll + 좌우 오프셋) |
| Space | 점프 |
| ` (백틱) | 콘솔 |

입력은 **Input Action 에셋 없이도** 동작합니다 (`UInputComponent::BindKey` 직접 바인딩).
캐릭터 BP의 `Input|CQB` 카테고리에 IA 에셋을 지정하면 해당 입력만 Enhanced Input 경로로 바뀝니다.

### 디버그 훅

디버그는 전부 `ACQBDebugDirector` 한 액터에 모여 있고, 커맨드라인으로만 동작합니다.
게임플레이 클래스에는 디버그 코드가 없습니다. 인자를 안 주면 이 액터는 아무 일도 하지 않습니다.

| 인자 | 동작 |
|---|---|
| `-CQBPlayerAt=X,Y,Z` | 플레이어를 그 자리에서 시작 (PlayerStart는 그대로) |
| `-CQBKillEnemyAfter=<초>` | N초 뒤 타격 — "Man down!" + 역할 재배정 확인 |
| `-CQBKillCount=<수>` `-CQBKillDamage=<비율>` | 몇 명을, 얼마나. 0.7이면 죽이지 않고 70%만 |
| `-CQBOrderAfter=<초>` `-CQBOrder=<이름>` `-CQBOrderDoor=<n>` | 분대 명령 / 외침 |
| `-CQBScreenshotAfter=<초>` | 스크린샷 |

```bash
ProjectTF.exe -game -nullrhi -unattended -stdout -CQBPlayerAt=200,80,120 -CQBKillEnemyAfter=8
```

---

## 2. 레벨 구조

**`/Game/CQB/Lvl_CQB`** — 엔진 기본 큐브로 그레이박싱. `Scripts/BuildCQBLevel.py`가 전부 생성합니다.
CQB 스케일(방 8m, 복도 2m, 엄폐물 110~120cm).

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

`╳` 가 개구부입니다. 설계 의도 두 가지:

**측면 복도 — 이 레벨의 핵심.** 우회로가 없으면 `Flank` 상태가 갈 곳이 없어서 AI가 결국
정면으로 밀고 들어옵니다. 이 복도 덕분에 Flanker가 방 B 남쪽 → 서쪽 통로 → 방 A 남서쪽으로
돌아 **플레이어 뒤/옆에서** 나타납니다.

**개구부 엇갈림.** 진입 복도는 방 A 남쪽(Y -350~-150), 방 B로 가는 문은 북쪽(Y 150~350)입니다.
일직선으로 뚫었더니 스폰 지점에서 방 B가 그대로 보여 적이 즉시 사격을 시작했고,
모서리 돌며 확인하는 구간이 통째로 사라졌습니다.

| 요소 | 값 |
|---|---|
| 방 A / 방 B | 각 800 × 800, X[-800,0] / X[200,1000], Y[-400,400] |
| 벽 높이 / 두께 | 320 / 40 |
| 개구부 폭 | 200 (2m) |
| 엄폐물 | 높이 110~120 큐브 5개 |
| PlayerStart | (-1200, -250, 100) |
| EnemySpawner | (600, 0, 20) — `ACQBSpawner`, 적 3명이 엄폐물 옆에 전개 |

레벨 재생성:
```bash
UnrealEditor-Cmd.exe ProjectTF.uproject -run=pythonscript -script="Scripts/BuildCQBLevel.py"
```

---

## 3. 클래스 구조

```
Source/ProjectTF/
├── ProjectTFCharacter        (ACharacter, abstract)  플레이어. BP_FirstPersonCharacter의 부모
│   ├── UHealthComponent                              체력
│   ├── UWeaponComponent                              히트스캔 무기
│   ├── UStaticMeshComponent                          1인칭 라이플 (HandGrip_R 소켓)
│   ├── UNavigationInvokerComponent                   주변 NavMesh 타일 생성
│   └── 입력: Fire / ADS / Reload / Lean(Q,E)
├── ProjectTFCameraManager    (APlayerCameraManager)  린을 최종 POV에 적용
├── ProjectTFPlayerController                         CQBHUD 강제 적용
│
└── CQB/
    ├── CQBTypes.h                EEnemyState / ESquadRole / ECalloutType + 문자열 헬퍼
    ├── UHealthComponent          MaxHealth, CurrentHealth, TakeDamage, OnDeath, OnHealthChanged
    │                             └ OnTakeAnyDamage 구독 → 엔진 데미지 경로도 수용
    ├── UWeaponData               (UPrimaryDataAsset) Damage, FireRate, MagSize, ReloadTime,
    │                             Recoil Pitch/Yaw Min·Max, ADSFov, HipFov, Range, Noise
    ├── UWeaponComponent          Fire / StartFire / StopFire / Reload / StartADS / StopADS
    │                             라인트레이스 → ApplyPointDamage → UHealthComponent
    │                             반동(컨트롤러 회전 가산 + 자동 회복), FOV 보간,
    │                             발사 시 UAISense_Hearing 노이즈 이벤트
    ├── ACQBHUD                   (AHUD) 캔버스 크로스헤어 + HP/탄약 + 디버그 명령
    ├── ACQBCharacter             (ACharacter) AI 폰의 공통 몸. 플레이어와 같은 Health/Weapon
    │                             컴포넌트 재사용, 사망 시 래그돌 → 6초 후 Destroy
    │                             진영은 Neutral — 어느 편인지는 하위 클래스가 정함
    │   ├── AEnemyCharacter       Faction = Enemy,  AIController = AEnemyAIController
    │   └── AAllyCharacter        Faction = Ally,   AIController = AAllyAIController
    ├── AEnemyAIController        (AAIController) 상태머신 + AIPerception(Sight/Hearing)
    │                             EQS 또는 NavMesh 샘플링으로 엄폐 지점 탐색
    ├── ASquadManager             (AActor, 월드 1개) 역할 배분 / 측면 지점 / 콜아웃 중계
    ├── ACQBSpawner               (AActor) CharacterClass를 N개 배치 + 시작 시 NavMesh 확인
    │                             레벨에 2개 (적 3명 / 아군 4명). 어느 편인지는 모름
    ├── ACQBDebugDirector         (AActor, 런타임 자동 생성) 커맨드라인 디버그 훅 전부
    └── UEnvQueryContext_CQBPlayer EQS에서 플레이어를 가리키는 컨텍스트
```

### 데이터 흐름

```
[입력] → AProjectTFCharacter → UWeaponComponent::Fire()
                                 ├→ LineTrace → ApplyPointDamage → UHealthComponent
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

## 4. 적 AI 상태 전이표

상태마다 `Enter / Update / Exit`가 있고, `Tick`이 현재 상태의 `Update`를,
`SetState()`가 `Exit(old) → Enter(new)`를 호출합니다.

| 상태 | Enter | Update | 전이 → 다음 상태 |
|---|---|---|---|
| **Idle** | 정지, 포커스 해제 | 대기 | 시각 자극 → `Engage`<br>총성 → `Investigate`<br>아군 "Contact!" → `Investigate` |
| **Investigate** | 마지막 자극 위치로 이동 | 도착 판정 + 대기 누적 | 시야 확보 → `Engage`<br>도착 후 3초 미발견 → `Idle` |
| **Engage** | 포커스, 사격 시작, 역할 요청 | 최소 1.2초 사격 | 역할=Flanker & 미수행 → `Flank`<br>그 외 → `Cover` |
| **Cover** | 사격 중지, 엄폐 지점 탐색 후 이동 | 도착 판정 | 도착 또는 지점 없음 → `Suppress` |
| **Suppress** | "Suppressing!" 콜아웃, 사격 | 2초 사격 / 1초 휴식 반복 | (전역 규칙으로만 이탈) |
| **Flank** | 측면 지점 이동 + "Flanking left/right!" | 도착 판정 | 도착 → `Engage` (재플랭크 금지) |

**전역 전이 규칙**

| 조건 | 적용 상태 | 결과 |
|---|---|---|
| 시야 3초 이상 상실 | `Engage` `Cover` `Suppress` | "Lost visual" → `Investigate` (마지막 목격 위치) |
| 플레이어 사망 | 전체 | `Idle` |
| 시야 확보 | `Idle` `Investigate` | `Engage` |

> **`Flank`는 시야 상실 규칙에서 제외했습니다.** 우회는 시야를 일부러 끊는 기동이고,
> 이 레벨의 측면 복도는 편도 약 22m(6초)라 규칙을 그대로 적용하면 모든 flank가 중간에 취소됩니다.
> 도착해 `Engage`로 돌아온 뒤부터 다시 적용됩니다.

시야 판정은 Perception(Sight, 반경 2000 / 시야각 70도)으로 **획득**하고,
매 프레임 `LineOfSightTo()`로 **유지**를 확인합니다. 엄폐물 뒤로 숨으면 즉시 반영됩니다.

---

## 5. 분대 (SquadManager)

### 역할 배분 `RequestRole()`

| 교전 진입 순서 | 역할 | 경로 |
|---|---|---|
| 1번째 | `Suppressor` | Cover → Suppress |
| 2번째 | `Flanker` (좌/우 교대) | Flank → Engage |
| 3번째 | `Suppressor` | Cover → Suppress |
| 4번째 | `Flanker` (반대쪽) | |

적 사망 시 `NotifyEnemyDied()` → 전원 역할 초기화 → 생존자가 재요청 → **역할 재배정**.

### 측면 지점 `GetFlankPoint()`

플레이어→적 벡터를 플레이어 기준 **좌/우 90도 회전**, 거리 600 지점을 `ProjectPointToNavigation()`으로
NavMesh에 투영. 배정된 쪽이 NavMesh 밖이면(실내 맵은 한쪽만 우회로가 있는 경우가 많음)
**반대쪽을 시도**하고, 둘 다 실패하면 플레이어 주변 도달 가능 지점으로 대체합니다.

### 콜아웃 `Broadcast()`

화면에 `[Enemy_1] Flanking left!` 형식으로 3초 표시 + 전 분대원에게 전달.

| 트리거 | 문구 |
|---|---|
| 최초 발견 | `Contact!` |
| Suppress 진입 | `Suppressing!` |
| Flank 진입 | `Flanking left!` / `Flanking right!` |
| 시야 3초 상실 | `Lost visual` |
| 아군 사망 | `Man down!` |

`Contact!`를 받은 Idle 상태 적은 발신자의 마지막 목격 위치로 `Investigate` 합니다.

---

## 6. 검증 결과

헤드리스 실행(`-game -nullrhi`) 로그로 확인한 실제 시퀀스입니다.

**교전 + 측면 우회** — 3회 실행 3회 재현
```
CQB callout: [Enemy_2] Contact!
CQB: Enemy_1  Idle -> Investigate          아군 콜아웃 수신
CQB: Enemy_3  Idle -> Investigate
CQB: Enemy_2  Engage -> Cover -> Suppress  1번째 = Suppressor
CQB callout: [Enemy_2] Suppressing!
CQB: Enemy_3  Engage -> Flank              2번째 = Flanker
CQB callout: [Enemy_3] Flanking left!
CQB: Enemy_1  Engage -> Cover -> Suppress  3번째 = Suppressor
CQB: Enemy_3  Flank -> Engage              측면 지점 도착
```

**시야 상실**
```
CQB callout: [Enemy_2] Lost visual
CQB: Enemy_2  Suppress -> Investigate
```

**아군 사망 + 역할 재배정**
```
CQB debug: hitting EnemyCharacter_0
CQB callout: [Enemy_1] Man down!
CQB: Enemy_2  Suppress -> Engage           역할 반납 후 재요청
```

---

## 7. 디버그 표시

| 위치 | 내용 |
|---|---|
| 화면 중앙 | 크로스헤어 — ADS 시 수축, 발사 시 확장, 명중 시 빨간 X |
| 화면 하단 | HP (좌) / 탄약·RELOADING (우) |
| 화면 좌상단 | 발사·명중·재장전·피격 메시지, 콜아웃 `[Enemy_1] Flanking left!` |
| 적 머리 위 | `Enemy_1  [Engage]  Suppressor  LOS` |
| 월드 | 탄도 라인(노랑), 명중(빨강 구), 벽 명중(흰 점) |
| 로그 `LogProjectTF` | `CQB: Enemy_1  Engage -> Cover`, `CQB callout: ...` |

---

## 8. 주요 파라미터

| 항목 | 기본값 | 위치 |
|---|---|---|
| 플레이어 체력 | 100 | `UHealthComponent::MaxHealth` |
| 플레이어 무기 | Damage 20 / FireRate 8 / Mag 30 / Reload 1.8s | `UWeaponData` |
| ADS / Hip FOV | 55 / 90 | `UWeaponData` |
| 반동 | Pitch 0.25~0.7, Yaw ±0.25, 회복 6°/s | `UWeaponData` |
| 적 체력 | 60 | `ACQBCharacter::MaxHealth` |
| 적 무기 | Damage 7 / FireRate 2.5 / 탄퍼짐 4° | `ACQBCharacter::Weapon*` |
| 항복 임계·가중치 | 1.0 / 거리 0.5 / 고립 0.4 / 시야없음 -0.3 | `AEnemyAIController::Compliance*` |
| 시야 / 청각 | 반경 2000·각도 70 / 3000 | `AEnemyAIController` |
| 시야 상실 유예 | 3초 | `LoseSightGraceTime` |
| 제압 사격 주기 | 2초 사격 / 1초 휴식 | `SuppressFire/RestDuration` |
| 측면 거리 | 600 | `ASquadManager::FlankDistance` |

전부 `EditAnywhere / EditDefaultsOnly`라 디테일 패널에서 조정 가능합니다.

---

## 9. 구현 노트

### NavMesh를 인보커로 생성하는 이유

레벨에 NavMesh 볼륨이 있고(bounds 2800×1600×800 등록 확인), 지오메트리도 navigation relevant이고,
NavMesh도 Dynamic 생성인데 **타일이 0개 생성되는** 문제가 있었습니다. 커맨드릿에서 굽는 것도
불가능합니다 — 커맨드릿은 navigation build lock을 겁니다
(`Navigation NOT building because navigation build is locked`).

그래서 **Navigation Invoker**로 전환했습니다. 플레이어와 적이 `UNavigationInvokerComponent`를
들고 있고, 주변 타일이 로드 시 생성됩니다(이 레벨 기준 약 1.3초).

그 지연이 두 번째 버그를 드러냈습니다: 타일 생성 전에 나간 이동 요청이 실패하는데,
상태머신이 **실패를 "도착"으로 처리**해서 적이 포기하고 Idle로 돌아갔습니다.
지금은 실패한 요청을 일정 간격으로 재시도하고, 재시도 예산을 넘겨야 포기합니다.

### 데미지를 엔진 파이프라인으로 보내는 이유

처음엔 `UHealthComponent::TakeDamage()`를 직접 호출했는데, 그러면 God 치트·데미지 타입·면역이
전부 무시됩니다. 지금은 `UGameplayStatics::ApplyPointDamage()` → `AActor::TakeDamage` →
`OnTakeAnyDamage` → `UHealthComponent` 순으로 흐릅니다.

### 템플릿 Variant

`Variant_Shooter`(발사체 무기 + StateTree AI)와 `Variant_Horror`는 **그대로 뒀습니다.**
CQB 구현은 기본 First Person 경로에 붙어 있고, 사격은 발사체가 아닌 **라인트레이스**입니다.
적 애님 `ABP_TP_Rifle`만 Shooter variant 에셋을 재사용합니다.

---

## 10. 빌드 / 패키징

```bash
# 에디터 타깃 (에디터 종료 후 — Live Coding이 잡고 있으면 UBT가 거부)
Build.bat ProjectTFEditor Win64 Development -Project="...\ProjectTF.uproject"

# 패키징 (Development, CQB 맵만 쿡, pak 압축, 심볼 제외)
RunUAT.bat BuildCookRun -project="...\ProjectTF.uproject" -noP4 -platform=Win64 \
  -clientconfig=Development -cook -build -stage -pak -compressed -archive \
  -archivedirectory="...\Packaged" -nodebuginfo
```

**`-clientconfig=Development`를 Shipping으로 바꾸지 마세요.** 실행 파일은 절반으로 줄지만
`ENABLE_DRAW_DEBUG`가 0이 되어 `DrawDebug*`가 통째로 컴파일에서 빠집니다. 적 머리 위 상태
표시, 콜아웃, 명령 바닥 마커 — 이 샘플이 보여주려는 것이 전부 사라집니다. Test 구성도 같습니다.

**아카이브 폴더는 먼저 비우세요.** 아카이브 단계는 기존 파일을 지우지 않습니다. 예전에 다른
구성으로 만든 실행 파일이 남아 그대로 같이 담깁니다. `-nodebuginfo`를 빼면 228MB PDB도 들어갑니다.
둘 다 겹쳐서 1.1GB가 나온 적이 있습니다.

**패키징 후에는 반드시 실행해서 눈으로 확인하세요.** 쿠커가 C++ 소프트 경로를 따라가지 못해
무기 메시와 애님 BP가 pak에서 빠진 적이 있습니다. 에디터에서는 멀쩡했고, 실패는 조용했습니다.
자세한 것은 CODE_GUIDE 10절.

```bash
# 12초 뒤 스크린샷. 적 3명 한가운데에서 시작하므로 교전·상태 표시가 한 장에 담깁니다
ProjectTF.exe -windowed -ResX=1280 -ResY=720 -CQBScreenshotAfter=12 -CQBPlayerAt=200,80,120
# 결과: %LOCALAPPDATA%\ProjectTF\Saved\Screenshots\Windows\
```


에디터를 켠 채 코드를 고쳤다면 커맨드라인 대신 **Ctrl+Alt+F11**(Live Coding).
단, **새 C++ 클래스를 추가했을 때는 Live Coding으로 등록되지 않아 에디터 재시작이 필요**합니다.

# Code reading guide

The order to read this sample's C++ in, and what to look at in each file. All of the gameplay
lives in `Source/ProjectTF/CQB/`; three files from the First Person template were modified and
nothing else was touched.

One line: **the enemies and the player's squad run the same state machine, and only their side
differs.**

> Korean: [CODE_GUIDE.md](CODE_GUIDE.md). The two are kept in step; this one is the translation.

---

## Reading order (about 40 minutes)

| # | File | Lines | Why here |
|---|---|---|---|
| 1 | `CQBTypes.h` | 205 | Faction, state, role and callout enums, plus the faction lookup. Everything else refers to it |
| 2 | `HealthComponent.h/.cpp` | 147 | The simplest one. Gets you used to the component pattern |
| 3 | `WeaponData.h` | 101 | Every weapon number, with units in the comments |
| 4 | `WeaponComponent.cpp` | 435 | The firing pipeline. `Fire()` alone is half of it |
| 5 | `EnemyAIController.h` | 347 | The machine's shape. The header alone shows the design |
| 6 | `EnemyAIController.cpp` | 399 | The machine itself — senses in, state out |
| 7 | `EnemyAIController_States.cpp` | 266 | Seven states, one Enter/Update/Exit trio each |
| 8 | `EnemyAIController_Actions.cpp` | 237 | The verbs states call — fire, face, move, find cover |
| 9 | `EnemyAIController_Squad.cpp` | 173 | Callouts, role reassignment, the surrender arithmetic |
| 10 | `AllyAIController.cpp` | 395 | Inherits 6–9 and adds five orders |
| 11 | `SquadManager.cpp` | 222 | Role handout and callout relay for the enemy side |

`AEnemyAIController` is one class across four files, split by role rather than by line count. If
you came to change how one state behaves, `EnemyAIController_States.cpp` is the only file you
need to open.

---

## 1. The whole thing on one page

```
                    ┌──────────────────────────┐
                    │  AEnemyAIController      │  perception + combat states
                    │  Idle/Investigate/Engage │  (Faction = Enemy)
                    │  Cover/Flank/Suppress    │
                    └───────────┬──────────────┘
                                │ inherits
                    ┌───────────▼──────────────┐
                    │  AAllyAIController       │  + five orders
                    │  Follow/Hold/Stack/Clear │  (Faction = Ally)
                    │  Watch                   │
                    └──────────────────────────┘

   ACQBCharacter ───┬── UHealthComponent           shared with the player
  (Faction=Neutral) ├── UWeaponComponent           shared with the player
        ▲           ├── UWeaponVisualComponent
        │
        ├── AEnemyCharacter   Faction = Enemy,  AIController = AEnemyAIController
        └── AAllyCharacter    Faction = Ally,   AIController = AAllyAIController
```

**Same components, same state machine, different side.** That is the whole design.

Both subclasses are about ten lines. The body — components, health, ragdoll on death, the
sight-target override — lives in `ACQBCharacter`, and the subclasses decide only **which side they
are on and which brain drives them**. That is why an ally is not a subclass of an enemy: an ally is
not an enemy, and a signature that carries one of them should not have to lie about what it takes.

The level places one spawner class, `ACQBSpawner`, twice. What separates them is `CharacterClass`:
`AEnemyCharacter` for the three in room B, `AAllyCharacter` for the player's four. The spawner does
not know which side it is filling.

---

## 2. UWeaponComponent — the firing pipeline

**`Fire()` is the whole thing.**

```
CanFire()          reloading / cooling down / dead?
  ↓
out of ammo        → start Reload() and stop
  ↓
GetViewPoint()     camera for the player, pawn view point for the AI
  ↓
FMath::VRandCone() spread by AimSpreadHalfAngle
  ↓
LineTraceSingleByChannel(ECC_Visibility)
  ↓
faction check      same side → "holding fire", no damage
  ↓
ApplyPointDamage() → AActor::TakeDamage → OnTakeAnyDamage → UHealthComponent
  ↓
ApplyRecoil()      added to controller rotation, taken back off later
  ↓
ReportNoiseEvent() the shot reaches the AI hearing sense
  ↓
SetTimer()         next shot, from FireRate
```

**Worth looking at**

- `GetViewPoint()` — camera for a player, pawn view point for an AI. This is what lets both sides
  share one component.
- `ApplyRecoil()` / `RecoverRecoil()` — recoil is added to the controller's rotation and the same
  amount is taken back off while not firing, so it is real aim disturbance rather than a visual.
- `GetWeaponData()` — builds a runtime default when no asset is assigned, which is why the sample
  runs with no data asset set up.
- The tracer is drawn from the **muzzle**, not from the view point the trace starts at. A thick line
  drawn from the camera to the point the camera is looking at is seen end on and renders as a solid
  slab over the crosshair.

---

## 3. AEnemyAIController — the combat state machine

### Shape

```cpp
Tick()
 ├─ UpdateSenses()             is the target still visible
 ├─ RetryFailedMove()          retry a failed path request
 ├─ UpdateGlobalTransitions()  rules that apply in any state   ← virtual
 ├─ UpdateState(CurrentState)  the current state's Update      ← virtual
 └─ DrawStateDebug()           the text over its head

SetState(New)
 ├─ ExitState(Current)   ← virtual
 ├─ CurrentState = New
 └─ EnterState(New)      ← virtual
```

`Enter/Update/ExitState` are **virtual functions wrapping a switch**. The ally controller overrides
them, handles its own orders and passes everything else to `Super::`. That is the single seam for
adding a state.

### How to read it

Read the trios in the order `Idle → Investigate → Engage → Cover → Suppress`. None of them is over
thirty lines.

### Four places worth the attention

**(1) `UpdateGlobalTransitions()` — the Flank exemption**

```cpp
if (IsInCombat() && CurrentState != ECQBAIState::Flank)
```

Flanking is a manoeuvre that deliberately breaks line of sight, so it is exempt from the "three
seconds without sight → Investigate" rule. Without the exemption every flank is cancelled halfway
down the side corridor, which is a 22 m round trip.

**(2) `HasReachedGoal()` + `RetryFailedMove()`**

Treating a failed path request as "arrived" makes an enemy give up and stand still. That happened
for real while the navmesh was still generating. A failure is not an arrival until the retry budget
(`MaxMoveRetries`) is spent.

**(3) `FindCoverPointFallback()`**

Cover search that works without an EQS asset: sample 24 points on the navmesh, keep the ones where a
trace from the player's eye height to the candidate is **blocked**, take the nearest of those.

**(4) `IsHostile()`**

Decided by `FCQBFactions::AreHostile()`. It used to hard-code "is this the player", which stopped
being true the moment the player had a squad. Dead actors are not threats and are excluded.

---

## 4. AAllyAIController — the squad member that takes orders

**Inherits** `AEnemyAIController`, so perception, Engage, Cover, Suppress and firing all come with
it. All an ally adds is a different faction, skipping registration with the enemy squad manager, and
five orders.

| Order | Key | Behaviour |
|---|---|---|
| `Follow` | Z | Holds 300 behind the player. Re-paths only once the player has moved 150 |
| `Hold` | H | Stops where it stands, still fights what it sees |
| `Stack` | aim at a door + 1 | Moves to its assigned left/right stack point, says "In position" on arrival |
| `Clear` | aim at a door + 2 | Goes through one at a time, each to its own slot, shooting on the move; three quiet seconds in the room → "Room clear!" → Follow |
| `Watch` | aim at a point + 3 | Stays put and keeps eyes on that point |

**The squad is four, split Red two and Blue two.** The mouse wheel cycles which element the next
order goes to: RED → BLUE → GOLD (everyone). Orders reach only the selected element, so Blue can
stack a door while Red holds the corridor. `AAllyAIController::OnPossess()` assigns the element by
parity and names them `RED_1` through `BLU_2`.

Each member also **wears its element colour** — pale red or pale blue, applied in the same
`OnPossess` that decided the element, so the colour and the callsign cannot disagree. Suspects stay
grey. An order draws an element-coloured ring on the floor for four seconds (`DrawOrderMarker`).

### How orders and combat interact

```
enemy spotted while carrying out an order → Engage (the parent state machine)
                                               ↓ fight ends (3 s without sight)
                                         back to StandingOrder
```

`StandingOrder` remembers the last order and returns to it when the fight ends. **`Clear` is the
exception** — entering the room *is* the order, so it fights from inside the state rather than
leaving it.

### The navmesh projection in `UpdateFollow()`

"Three metres behind the player" is inside a wall a lot of the time indoors — always, if the player
has their back to one. The point is projected onto the navmesh, and falls back to the player's own
position when projection fails.

---

## 5. Surrender and compliance — why this is not a shooter

Aim at a suspect and press **F** and the player shouts "Drop the weapon!". The AI decides on the
spot, in `AEnemyAIController::ReceiveChallenge()`.

### `EvaluateCompliance()`

| Term | Weight | Reasoning |
|---|---|---|
| Wounded | up to 1.0 | In proportion to how far below 60 % health they are |
| Shouting distance | up to 0.5 | Closer is more convincing, out to 1200 |
| Isolated | +0.4 | Everyone on their side is dead or has given up |
| Cannot see the challenger | −0.3 | Someone you cannot see is less frightening |
| Being aimed at | +0.35 | The player is pointing a weapon while shouting |

Over `ComplianceThreshold` (1.0) they surrender.

### Measured, three headless runs

Same spot, varying only the wound. The command below runs it.

```
full health   →  0.74 / 1.00   "Not a chance!"   refused
30 % health   →  1.18 / 1.00   "Hands up!"       Cover -> Surrender
15 % health   →  1.48 / 1.00   "Hands up!"       Suppress -> Surrender
```

The decimals move a little between runs, because the distance term depends on exactly where the
suspect happens to be standing when the shout lands. The outcomes do not.

```bash
for D in 0.0 0.7 0.85; do
  ProjectTF.exe ... -CQBPlayerAt=200,80,120 \
    -CQBKillEnemyAfter=3 -CQBKillCount=3 -CQBKillDamage=$D \
    -CQBOrderAfter=6 -CQBOrder=challenge
done
```

**It reads the situation rather than tossing a coin.** The case that holds at 0.74 is what the
system is worth. The same wound shouted at from across the building does not land, because the
distance term is worth up to half the threshold on its own.

### After a surrender

`Surrender` is **terminal** — blocked on the first line of `UpdateGlobalTransitions`. The weapon is
put away, the capsule drops to a kneel, and `IsHostile()` returns false so **the AI on both sides
stops shooting at them**. That is what stops the squad from executing someone with their hands up.
A surrendering suspect hands back its squad role, and the survivors get reassigned.

---

## 6. ADoorwayMarker — what an order is given to

The actor's **forward vector is the entry direction**. Rotating it in the editor is all it takes to
aim one.

```
GetStackPoint(Left/Right)   55 cm to the side, 110 cm back
GetClearPoint()             450 cm past the opening
```

**Why 55 cm to the side:** the corridors are 2 m wide, so 100 would put a squad member exactly
inside the wall.

**Why it outlines itself:** nothing on screen used to say which openings take orders. The hint at
the bottom of the HUD only appears once the crosshair is already on one, so finding them meant
sweeping the room and watching for text. Each marker now draws its opening — a thin blue frame while
it sits there, thick orange when the crosshair is on it, and when aimed at, a sphere on each stack
point and a line to the point inside the room that Clear sends them to. The marker is *told* whether
it is aimed at by `UpdateAimedDoorway`; letting a doorway look up the player pawn would pull the
player class into a file that otherwise only knows about points in space.

**Why it has no collision (important):** it started with a box to trace against for aiming, and that
box blocked `ECC_Visibility`. **The AI sight sense uses the same channel.** Putting a marker in a
doorway blocked the line of sight of every AI trying to see through that doorway. Now
`AProjectTFCharacter::UpdateAimedDoorway()` picks geometrically by crosshair angle and spends one
trace confirming the doorway is not behind a wall.

---

## 7. ASquadManager — the enemy squad

Three functions. **Enemies only** — the player's squad answers to the player and does not register
(`ShouldJoinSquad()` returns false for allies).

| Function | What it does |
|---|---|
| `RequestRole()` | By how many already hold one: even goes Suppressor, odd goes Flanker, alternating sides |
| `GetFlankPoint()` | ±90° off the player→enemy vector, 600 out, projected to navmesh. Falls back to the other side, then to random |
| `NotifyEnemyDied()` | "Man down!" → clear everyone's role → survivors ask again |

`ReassignRoles()` is **two passes** on purpose. In one pass the first enemy to ask again counts the
others while they are still holding their old roles. Everything has to be cleared before anyone
re-requests.

---

## 8. The three template files that were modified

| File | What changed |
|---|---|
| `ProjectTFCharacter.h/.cpp` | Health/Weapon/WeaponVisual components, fire/ADS/reload/lean input, squad order input, faction and sight-target interfaces |
| `ProjectTFCameraManager.cpp` | Applies lean (roll plus a lateral offset) to the final POV in `UpdateViewTarget()` |
| `ProjectTFPlayerController.h/.cpp` | `ClientSetHUD(ACQBHUD)` in `BeginPlay`, team id |

`Variant_Shooter` and `Variant_Horror` are **untouched** — verifiable with `git diff`.

**Why lean is done in the camera manager:** the camera has `bUsePawnControlRotation=true`, so roll
written to the component is overwritten every frame. It has to be added after the final POV is
decided.

---

## 9. Design choices and the reasons

| Choice | Reason |
|---|---|
| A hand-written state machine rather than a Behavior Tree | Required. Every transition rule is visible in code and traceable in the log |
| The ally controller **inherits** the enemy one | One copy of the combat logic. An ally adds a faction and five orders |
| Player and AI share the same components | One copy of health and weapon logic. The AI just calls `StartFire()` |
| Damage goes through `ApplyPointDamage` | Calling the health component directly bypasses god mode, damage types and immunity |
| The weapon is attached to the camera | On a hand socket it does not follow the view, because the template's arm animation wants aim data the character does not supply |
| Input bound directly with `BindKey` | Works with no Input Action assets. Assigning an IA takes precedence |

---

## 10. Traps — the ones that actually cost days

Written down so the next person does not fall in the same holes.

**AI sight traces to the actor origin, which is the waist.** With cover at 110–120 cm, someone whose
head and chest are in plain view is judged completely hidden. `LineOfSightTo()` uses eye height and
says the opposite, so the two disagree. → `CQBSightTarget` and `IAISightTargetInterface` test
**three points: eyes, chest, waist.**

**`bGenerateNavigationOnlyAroundNavigationInvokers=True` makes `Build()` produce zero tiles.** The
volume is fine, the geometry is fine, and no navmesh appears at all. It is False here.

**Commandlets hold the navigation build lock** (`navigation build is locked`). You cannot bake
navigation headlessly.

**Loading a Blueprint from a constructor deadlocks the async loader**
(`Loading is stuck, flush will never finish`). Everything is `TSoftObjectPtr` loaded in `BeginPlay`.

**`SetVisibility(false, true)` hides children too** — including the weapon, which is a child of the
camera.

**The level script overwrites the map wholesale.** Once you start hand-editing it, do not run it
again.

**The navmesh erodes every obstacle by the agent radius, so cover near a doorway can seal it.** Cover_B1
stood across room B's entry lane; eroded by 35 cm, its edge met the eroded edge of the door's wall
stubs with no gap at all. The door stayed open to the player and shut to the AI, every route between
the rooms went round the flank corridor, and nothing looked wrong because every log line was a state
transition. `-CQBTraceSquad` exists because of this: a path of 3,040 for a trip that is 800 in a
straight line is the tell.

**An AI controller does not outlive its pawn.** It unpossesses and destroys itself the moment the
pawn dies, so **a list built from controllers cannot show anyone as down** - they vanish from it on
the next frame. That is why the HUD roster is built from pawns, and why the callsign and element are
stamped onto the pawn at possession.

**A corpse locks the state machine if you keep referencing it.** Bodies stay where they fall, so if
the "a dead target ends the fight" rule does not clear the reference, that branch is true on every
tick from then on and nothing below it runs again. An AI that wins a fight stops for good.

**The cooker cannot follow a soft path that only exists in C++.** This project is
`bCookMapsOnly=True`, so the cooker starts at `Lvl_CQB` and takes what it can reach by reference.
The weapon mesh and the animation blueprint exist only as **strings** inside C++
(`TSoftObjectPtr`), which is not reachable from the level. The failure is silent: the asset is not
in the pak, `LoadSynchronous()` returns null, and the game carries on as if nothing happened —
**with no weapon in hand and the AI standing in a bind pose.** → the folders have to be listed under
`DirectoriesToAlwaysCook` in `DefaultGame.ini`.

> It works in the editor and only breaks in a package. Shipping compiles the logging out, so you do
> not even get the warning. **There is no way to catch this except running the packaged build and
> looking at it.**

**`DrawDebug*` takes a lifetime in seconds, and passing `DeltaSeconds` means the shape expires
before the frame it was submitted for is drawn.** It looks exactly like the draw call doing nothing.
`-1` is the value that means one frame.

**Screenshots taken without UI drop the debug text.** `DrawDebugString` writes to the HUD's
`DebugCanvas`, which `RequestScreenshot(false)` leaves out, while the HUD's own canvas comes
through — so the shot looks complete and is missing precisely the thing being verified.

---

## 11. Running it yourself

Every debug hook lives in one actor, `ACQBDebugDirector`. None of it is in the gameplay classes. The
actor is not placed in the level; the spawner brings it up with `EnsureExists()`, and it does
nothing at all unless an argument asks for something.

| Argument | Meaning |
|---|---|
| `-CQBPlayerAt=X,Y,Z` | Puts the player there half a second in. Tests from a given spot without moving the PlayerStart |
| `-CQBKillEnemyAfter=<s>` | Hit suspects after a delay |
| `-CQBKillCount=<n>` | How many, nearest the player first. Default 1 |
| `-CQBKillDamage=<f>` | Fraction of max health. Default 10.0, which is lethal; 0.7 wounds instead |
| `-CQBKillSide=<side>` | `enemy` (default) or `ally`, for checking the down state on the HUD |
| `-CQBTraceSquad=<s>` | Every `<s>` seconds, log each squad member's state, position and path (point count, length, goal) |
| `-CQBOrderAfter=<s>` | Issue a squad order after a delay |
| `-CQBOrder=<name>` | follow / hold / stack / clear / watch / challenge |
| `-CQBOrderDoor=<n>` | 0 = room A door, 1 = room B door, ordered west to east |
| `-CQBScreenshotAfter=<s>` | Take a screenshot after a delay |

```bash
# the whole AI sequence, log only
ProjectTF.exe -game -nullrhi -unattended -stdout

# engagement, role handout, flank, a death and the reassignment, in one run
ProjectTF.exe ... -CQBPlayerAt=200,80,120 -CQBKillEnemyAfter=8

# squad orders
ProjectTF.exe ... -CQBOrder=stack -CQBOrderAfter=5 -CQBOrderDoor=1
ProjectTF.exe ... -CQBOrder=clear -CQBOrderAfter=5 -CQBOrderDoor=1

# surrender
ProjectTF.exe ... -CQBPlayerAt=200,80,120 \
  -CQBKillEnemyAfter=3 -CQBKillCount=3 -CQBKillDamage=0.7 \
  -CQBOrderAfter=6 -CQBOrder=challenge

# screenshot
ProjectTF.exe -game -windowed -CQBScreenshotAfter=5

# if you need to debug perception
log LogProjectTF Verbose
```

Everything is traceable from the lines beginning `CQB` in the log. Note that a headless run spends
close to fifteen seconds loading the map, so the process needs to stay alive well past whatever
`-CQBOrderAfter` is set to.

---

## 12. Verified sequences (headless logs)

**Enemy squad engagement**

```
[Enemy_1] Contact!                     sight acquired, relayed to the squad
Enemy_2, Enemy_3  Idle -> Investigate  heard the callout, moving
Enemy_1  Engage -> Cover -> Suppress   first to ask = Suppressor
Enemy_2  Engage -> Flank               second = Flanker
[Enemy_2] Flanking left!
Enemy_2  Flank -> Engage               arrived at the flank point
Enemy_3  Engage -> Cover -> Suppress   third = Suppressor
```

**Surrender**

```
CQB debug: player placed at V(X=200.00, Y=80.00, Z=120.00)
CQB debug: scripted order 'challenge' on Room A
Enemy_1  Cover -> Surrender
Enemy_1 surrendered (1.18 of 1.00)
```

**Squad orders**

```
scripted order 'stack' on Room B
RED_1/2, BLU_1/2  Follow -> Stack   →  all four reach In position

scripted order 'clear' on Room B
RED_1/2, BLU_1/2  Follow -> Clear
[RED_1] Contact! → Engage → Cover → Suppress      combat interrupts the order
[BLU_1] Room clear! → Clear -> Follow             the rest finish the room
RED_1  Suppress -> Clear                          and the order resumes after

scripted order 'watch' on Room B
RED_1/2, BLU_1/2  Follow -> Watch   →  all four say "Watching that"
```

**Lost sight and a death**

```
[Enemy_2] Lost visual      Enemy_2  Suppress -> Investigate
[Enemy_1] Man down!        Enemy_2  Suppress -> Engage (asks for a role again)
```

---

## 13. Where each kind of change goes

```
┌─ Config (.ini)          project-wide. Needs a restart
├─ Level (Lvl_CQB)        placement. Immediate, in the editor
├─ Data Asset             bundles of numbers. No recompile
└─ C++                    rules and state machine — the only real "logic"
```

**The principle:** "an enemy gives up after three seconds" is a rule and belongs in C++. The three
is a number and belongs in data.

### What to change where

| To change | Where | Recompile |
|---|---|---|
| Weapon numbers | a `UWeaponData` asset → the character BP's Weapon Component | no |
| Enemy health and weapon | `BP_Enemy` (inherits AEnemyCharacter) | no |
| Compliance weights (how hard surrender is) | the six Compliance* properties on `BP_EnemyAIController` | no |
| AI perception and combat timings | `BP_EnemyAIController` | no |
| Squad order distances and timings | `BP_AllyAIController` (inherits AAllyAIController) | no |
| Element colours | `RedTint` / `BlueTint` on `BP_AllyAIController` | no |
| Movement speed | the Character Movement component's Max Walk Speed on the character BP | no |
| How long bodies stay | `DeferredDestructionTime` on the character BP. Zero leaves them | no |
| Doorway highlight colours and size | the `DoorwayMarker` in the level, under Doorway\|Highlight | no |
| Flank distance and callouts | the `SquadManager` placed in the level | no |
| Door positions and stack points | the `DoorwayMarker` in the level (`bDrawDebug` shows the points) | no |
| Enemy/ally count and positions | `EnemySpawner` / `AllySpawner` in the level (both `ACQBSpawner`) | no |
| Level shape | `Scripts/BuildCQBLevel.py` | no (regenerates the level) |
| Transition rules, role handout | `CQB/*.cpp` | **yes** |

### Making the Blueprint shells

To get at the AI numbers you need the shells once:

```
Blueprint Class → AEnemyAIController  → BP_EnemyAIController
Blueprint Class → AEnemyCharacter     → BP_Enemy
  Class Defaults → AI Controller Class = BP_EnemyAIController
EnemySpawner in the level → Character Class = BP_Enemy
```

Same for the allies (`AAllyAIController` / `AAllyCharacter`).

### Three things to know

- **`SquadManager` is created at runtime if the level has none.** To change its numbers you have to
  place one in the level yourself.
- **There is one spawner class, `ACQBSpawner`.** Two are placed, named EnemySpawner and
  AllySpawner, and the only difference is `CharacterClass`. The spawner does not know which side it
  is filling.
- **`ACQBDebugDirector` is also created at runtime**, and does nothing without command line
  arguments. Do not go looking for debug code in the gameplay classes — all of it is here.

---

## 14. What is deliberately not open yet

- **No Blueprint extension hooks** (no `BlueprintImplementableEvent` anywhere under `CQB/`; the
  hits you will find are in the untouched template variants) — there is nowhere to hang
  sound or a muzzle flash from a Blueprint. The places to add them would be `OnStateChanged`,
  `OnCalloutSpoken`, `OnWeaponFired`, `OnEnemyDied`.
- **No enemy profile data asset** — a dozen AI numbers sit on the controller, so changing an
  enemy's temperament means duplicating a Blueprint rather than assigning an asset.
- **The AI numbers need a Blueprint shell first** — per the table above, the inspector does not open
  until you have made one.
- **The animation is the template's** — firing, being hit and surrendering are expressed by moving
  the capsule and hiding the weapon rather than by dedicated montages. The logic is right; only what
  you see is a placeholder.
- **No audio at all.** Nothing in this project plays a sound.

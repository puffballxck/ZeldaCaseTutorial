# ABP_Link 动画蓝图总结

> 读取日期：2026-09-09  
> 资产：`/Game/_Game/Animations/ABP_Link`  
> 生成类：`ABP_Link_C`  
> 父类：`UZCAnimInst`  
> 目标骨架：`/Game/_Game/Animations/LinkAnim/Link_Ani_Skeleton`  
> 资产状态：读取时 `is_dirty=false`，当前保存资产与编辑器状态一致

## 1. 资产定位

`ABP_Link` 的主要职责是组织 Link 的基础移动、跳跃、滑翔、锁定移动、装备姿势和武器动作姿势，并在最外层提供 Montage Slot。

运行时数据由父类 `UZCAnimInst` 的原生代码更新，当前 AnimBP 本身没有定义 Blueprint 成员变量。当前可见的函数和事件为：

- `AnimGraph`：已实现。
- `BlueprintUpdateAnimation`：存在事件节点，但当前事件图中没有有效执行链。
- `TryGetPawnOwner`：存在节点，但没有连接到事件或其他节点。
- `BlueprintThreadSafeUpdateAnimation`：可用但未实现。

因此，当前资产不是通过 EventGraph 每帧读取 Pawn 并写入变量，而是依赖 `UZCAnimInst::NativeUpdateAnimation()` 更新父类暴露的属性，AnimGraph 直接读取这些属性。

## 2. 原生动画数据来源

对应代码：

- `Source/ZCase/Public/Animations/ZCAnimInst.h`
- `Source/ZCase/Private/Animations/ZCAnimInst.cpp`

`NativeUpdateAnimation()` 当前提供以下运行时数据：

| 属性 | 当前来源 / 含义 | 在 ABP_Link 中的用途 |
|---|---|---|
| `GroundSpeed` | 角色水平速度 `VSizeXY(Velocity)` | 普通移动 BlendSpace 的速度输入；锁定移动 BlendSpace 的 Y 输入 |
| `AirSpeed` | 角色速度 Z 分量 | 对外可用；具体嵌套状态转场用途未由本次结构读取完整展开 |
| `bShouldMove` | 非下落、水平速度大于 5 且存在移动加速度 | 控制装备移动上半身层 |
| `bIsFalling` | `CharacterMovementComponent::IsFalling()` | 禁止下落期间使用锁定移动 BlendSpace |
| `bIsGliding` | `CurrentMT == MT_Gliding` | 在滑翔状态机和跳跃状态机之间选择 |
| `bReadyToThrow` | 角色的投掷准备状态 | 控制 `Lift_Wait` 上半身层 |
| `bWeaponEquipped` | Combat 的 `IsWeaponEquippedForAnimation()` | 控制装备待机、装备移动和锁定装备姿势 |
| `bGuarding` | Combat 的 `IsGuardPoseActive()` | 当前顶层 AnimGraph 未找到对应 Bool 节点 |
| `bIsTargetLocked` | 当前目标存在且实现目标锁定接口、且允许被锁定 | 控制锁定移动和锁定待机姿势 |
| `LockOnDirection` | 水平速度相对角色 Yaw 的局部角度，范围 `-180~180` | 锁定移动 BlendSpace 的 X 输入 |

类默认值中速度和角度为 `0`，所有状态 Bool 默认值为 `false`。这些是 AnimInstance 的 CDO 默认值，不代表 PIE 中的实时值。

## 3. 状态机结构

当前 `AnimGraph` 包含三个状态机：

### 3.1 `Locomotion`

- `Idle`
- `Walk/Run`
- 2 个状态转场

`Idle` 状态的编辑器结构为：

```text
Nml_Wait
Pose_Sword_Wait
        \ /
Blend Poses by Bool (bWeaponEquipped)
        |
Output Animation Pose
```

装备后使用 `Pose_Sword_Wait`，未装备时使用 `Nml_Wait`。`Pose_Sword_Wait` 保留在 `Locomotion -> Idle` 中，没有被移动到独立的 Combat 状态机。

`Walk/Run` 状态使用 `BS_New` BlendSpace，编辑器中可见 `GroundSpeed` 直接连接到 BlendSpace 的速度输入。

### 3.2 `WithJumping`

包含以下状态：

- `Locomotion`
- `Jump`
- `FallLoop`
- `Land`
- 6 个状态转场

该状态机负责将基础移动与跳跃、下落、落地动作组合起来。嵌套状态图的完整转场表达式本次未通过结构化 DSL 读出，因此这里仅记录已确认的状态和资产引用，不臆测每个转场的精确条件。

### 3.3 `Gliding`

包含以下状态：

- `StartGliding`
- `GlidingLoop`
- 1 个状态转场

外层通过 `bIsGliding` 在 `Gliding` 和 `WithJumping` 两路姿势之间进行 Blend，Blend Time 读取为 `0.2 / 0.2` 秒。

## 4. AnimGraph 主链

当前最外层姿势链可以概括为：

```text
Locomotion State Machine
        |
Save Cached Pose: LocomotionCache

WithJumping State Machine ----\
                               Blend by Bool: bIsGliding
Gliding State Machine ---------/
        |
Save Cached Pose: WithGliding
        |
锁定移动选择
  条件：bIsTargetLocked && !bIsFalling && !bIsGliding
  True/Lock 路：BS_LockOn_Normal
      X = LockOnDirection
      Y = GroundSpeed
  另一条路：WithGliding
        |
Save Cached Pose: LockOnBaseCache
        |
装备移动选择
  条件：bWeaponEquipped && bShouldMove && !bIsTargetLocked
  装备移动路：上半身 Layered Blend + Sword_Move_Run_Upper
  另一条路：LockOnBaseCache
        |
Save Cached Pose: ArmedMoveCache
        |
锁定装备待机选择
  条件：bIsTargetLocked && bWeaponEquipped
  锁定待机路：上半身 Layered Blend + Sword_Lockon_Wait
  另一条路：ArmedMoveCache
        |
Save Cached Pose: CombatLocomotionCache
        |
投掷准备选择
  条件：bReadyToThrow
  准备动作路：上半身 Layered Blend + Lift_Wait
  另一条路：CombatLocomotionCache
        |
Save Cached Pose: PreWeaponActionCache
        |
Slot: WeaponAdditive (Group: ZCaseGroup)
        |
Layered Blend per Bone
        |
Slot: FullBody (Group: ZCaseGroup)
        |
Output Pose
```

### 4.1 锁定移动

`BS_LockOn_Normal` 使用：

- X：`LockOnDirection`
- Y：`GroundSpeed`
- Play Rate：`1.0`
- 循环：开启
- Blend Space：`/Game/_Game/Animations/LinkAnim/BS_LockOn_Normal`

只有在目标锁定、角色不在下落且不处于滑翔状态时，锁定移动路才会参与外层选择。

锁定移动结果被缓存为 `LockOnBaseCache`，后续装备移动、锁定待机和投掷准备都建立在这份基础姿势上。

### 4.2 装备移动上半身层

`Sword_Move_Run_Upper` 通过 `Layered Blend per Bone` 叠加到 `LockOnBaseCache` 上，动画引用为：

`/Game/_Game/Animations/LinkAnim/00_Combat/07_Weapon_State/Sword/Sword_Move_Run_Upper`

触发条件为：

```text
bWeaponEquipped && bShouldMove && !bIsTargetLocked
```

该层的四个 `Layered Bone Blend` 节点都使用相同的分支过滤配置：

- `Clavicle_L`，Blend Depth `0`
- `Clavicle_R`，Blend Depth `0`
- Blend Weight `1`
- Blend Mode：`Branch Filter`

因此它是从左右锁骨分支开始的上半身层，而不是把整套移动姿势替换成全身动作。

结果缓存为 `ArmedMoveCache`。

### 4.3 锁定装备待机

`Sword_Lockon_Wait` 叠加在 `ArmedMoveCache` 上，引用为：

`/Game/_Game/Animations/LinkAnim/00_Combat/03_LockOn/Sword/Sword_Lockon_Wait`

触发条件为：

```text
bIsTargetLocked && bWeaponEquipped
```

结果缓存为 `CombatLocomotionCache`。

### 4.4 投掷准备姿势

`Lift_Wait` 叠加在 `CombatLocomotionCache` 上，引用为：

`/Game/_Game/Animations/LinkAnim/Lift_Wait`

触发条件为：

```text
bReadyToThrow
```

结果缓存为 `PreWeaponActionCache`。这说明投掷准备姿势位于基础移动、装备移动和锁定待机之后，但位于最终 Montage Slot 之前。

## 5. Montage Slot 层级

最外层当前只有两个 Slot：

| Slot | Group | 位置 | 作用 |
|---|---|---|---|
| `WeaponAdditive` | `ZCaseGroup` | `PreWeaponActionCache` 之后 | 为武器附加动作提供上半身入口 |
| `FullBody` | `ZCaseGroup` | 最终输出之前 | 最终全身 Montage 入口 |

当前链为：

```text
PreWeaponActionCache
        |
  ┌─────┴────────────────┐
  |                      |
Base Pose       Slot: WeaponAdditive
  |                      |
  └─ Layered Blend per Bone
        |
Slot: FullBody
        |
Output Pose
```

`WeaponAdditive` Slot 的输出经过左右锁骨分支过滤后才叠加到基础姿势；`FullBody` Slot 位于最后，因此全身 Montage 拥有更高的最终覆盖优先级。

从当前结构可以得到的覆盖顺序是：

```text
FullBody Montage
    > WeaponAdditive / 上半身武器动作
    > ReadyToThrow / Lift_Wait
    > LockOn Wait 或 Armed Move
    > LockOn / Jump / Glide 基础姿势
```

两个 Slot 的 `bAlwaysUpdateSourcePose` 均为 `false`。当前未发现第二个 `FullBody` Slot。

## 6. 当前资产中值得注意的节点

### 6.1 `Guard_Wait` 尚未接入主输出链

当前资产依赖 `Guard_Wait` 动画，并且在顶层 `AnimGraph` 中存在 `SequencePlayer'Guard_Wait'`，但该节点的 Pose 输出没有连接到任何节点。当前顶层图也没有找到 `bGuarding` Bool Getter。

这与原生类中 `bGuarding = Combat->IsGuardPoseActive()` 的数据更新已经存在形成对照：运行时数据会更新，但当前 `ABP_Link` 还不会因为 `bGuarding` 自动切换到 `Guard_Wait`。

### 6.2 发现一条未接入输出的 Root Modify Bone 链

顶层图中还存在以下独立链：

```text
Sequence Player（未指定动画）
    -> Local To Component Space
    -> Transform (Modify) Bone: Root
    -> Component To Local Space
```

该链的输出没有继续连接到最终 Root。读取到的 Root 修改参数包括：组件空间、Additive Rotation、Roll `90` 度、Additive Scale `(100,100,100)`、Alpha `1`。由于它不在最终输出链上，本次不把它视为当前生效行为；如需清理或重新接入，应先在编辑器中确认设计意图。

## 7. 资产依赖与引用关系

### 7.1 主要动画和 BlendSpace 依赖

- `BS_New`
- `BS_LockOn_Normal`
- `Nml_Wait`
- `Pose_Sword_Wait`
- `Jump`
- `Fall_Loop`
- `Land`
- `Float`
- `Float_On`
- `Lift_Wait`
- `Guard_Wait`
- `Sword_Lockon_Wait`
- `Sword_Move_Run_Upper`

### 7.2 当前已知引用者

资产读取到的引用者为：

- `/Game/_Game/Blueprints/BP_Player`
- `/Game/_Game/Animations/ABP_Hair2`

### 7.3 运行时行为不属于本 AnimBP 的部分

以下职责不应从当前 `ABP_Link` 图中推断：

- 武器 Socket 的切换和装备生命周期
- 攻击 Montage 的触发与结束
- Weapon Trace、命中检测和伤害结算
- 目标锁定目标管理
- 角色投掷状态的业务维护

这些行为分别由角色、Combat、Target Lock、Notify 或原生 AnimInstance 数据源负责；`ABP_Link` 只消费结果并组织姿势。

## 8. 读取证据与验证边界

本次总结使用了当前编辑器资产的结构化读取、AnimGraph 编辑器视图、`ABP_Link` CDO 属性读取，以及 `ZCAnimInst.h/.cpp` 源码对照。

- 已确认：资产路径、父类、目标骨架、状态机名称、动画节点引用、缓存姿势、Bool 选择条件、Blend Time、Slot 层级和 Branch Filter 骨骼配置。
- 已确认：资产读取时未保存脏改。
- 已确认：`EventGraph` 当前没有有效更新链。
- 未执行：编译、PIE、动画预览运行时验证、Montage 实际覆盖效果验证。
- 因此，本文描述的是“当前保存资产的结构行为”，不等同于已经证明 PIE 中每个状态和动作都能正常播放。

如果后续要做运行时验收，建议按以下顺序观察：普通 Idle/Walk、装备 Idle/Walk、目标锁定移动、跳跃/下落/落地、滑翔、投掷准备、`WeaponAdditive` Montage、`FullBody` Montage，并重点确认 Root/Pelvis/腿部没有被上半身层误覆盖。

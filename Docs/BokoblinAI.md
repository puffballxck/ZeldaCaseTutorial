# Bokoblin AI

这版使用 Sight + Behavior Tree 完成小型战斗演示。生命值和伤害结算沿用 `ZCAttributeComponent`、`AZCEnemyBase` 与 `ZCCombatComponent`。

## 运行时分工

- `AZCBokoblinAIController`：Sight 感知、TargetActor、Focus、Possess 时记录出生点并启动 BT；超出活动范围时接管返程。
- `AZCBokoblinEnemy`：攻击目标与距离检查、停止移动、选择攻击 Montage。
- `UZCCombatComponent`：攻击生命周期、Notify 窗口、分段 Sphere Sweep、单次攻击去重、ApplyPointDamage。Bokoblin 用右手骨骼作为采样端点，玩家继续用武器 Socket。
- `UZCBTTask_Attack`：调用攻击入口并等待结束 Delegate；每个 AI 独立实例，Abort 时解绑并关闭攻击。
- `UZCBTTask_Chase`：复用引擎 MoveTo，只在任务启动时设置追击速度和攻击距离；不使用每帧距离 Service。
- `UZCBTTask_FindPatrolPoint`：在当前位置周围 800 cm 选择可到达导航点。
- `UZCBokoblinAnimInstance`：从 Velocity.Size2D 更新 Speed，AnimBP 自行处理基础动作。

死亡和受击沿用 EnemyBase，未新增对应 BT 分支。死亡会停止移动、清理 Focus、终止 Brain，并禁用攻击与感知。

## 编辑器资产

- `/Game/_Game/AI/Bokoblin/BB_Bokoblin`：业务键只有 TargetActor（Actor）和 PatrolLocation（Vector）；均不跨实例同步。引擎可能显示默认 SelfActor 键。
- `/Game/_Game/AI/Bokoblin/BT_Bokoblin`：Combat 优先，Patrol 回退；黑板观察者负责中断。
- `/Game/_Game/Animations/Enemy/Bokoblin/BS_Bokoblin_Locomotion`：0 Idle、180 Walk、400 Run。
- `/Game/_Game/Animations/Enemy/Bokoblin/ABP_Enemy_Bokoblin`：Speed → BlendSpace → 原有 DefaultSlot → Output。
- `/Game/_Game/Animations/Enemy/Bokoblin/Animation/Montage/AM_Bokoblin_Attack_01`：右拳攻击，Damage 轨道使用现有 ZC Weapon Trace NotifyState。
- `/Game/_Game/Blueprints/Enmies/BP_Enemy_Bokoblin`：继承 ZCBokoblinEnemy；Auto Possess AI 为 Placed in World or Spawned。

保留原有 HitReact 和 Death Montage。Death 保留不自动 Blend Out 的末帧停留行为。

## 调整与演示

Sight 默认 1800 cm，Lose Sight 2200 cm，半视野角 70 度。巡逻 180 cm/s，追击 400 cm/s。攻击距离用角色中心的平面距离；Chase 关闭额外胶囊半径，保证 MoveTo 与 TryAttack 使用同一口径。

在 TestLevel 中 Play，靠近 Bokoblin 正面让它发现玩家；后退观察追击，进入近距离观察右拳攻击，离开视野观察恢复巡逻。用玩家原有攻击检查受击和死亡。

右拳接触帧通过 Montage NotifyState 调整。扩大 AttackRange 不会扩大实际伤害：只有手部 Sweep 真实碰到玩家才会扣血。

## 重建与验证

编辑器关闭后编译：

```powershell
& F:/UE_5.8/Engine/Build/BatchFiles/Build.bat ZCaseEditor Win64 Development F:/Utorrent/ZCase/ZCase.uproject -WaitMutex -NoHotReloadFromIDE
```

资产初始化工具使用引擎 API 生成 BT 图、BlendSpace 和 Montage，并接入已存在的 Bokoblin Blueprint、AnimBP 与 TestLevel。仅需要初始化时运行；先保存并备份这些已有资产。生成后应直接在编辑器里调整资产。重跑会保留已存在的 BT、BlendSpace 和攻击 Montage，但会重新应用 Blueprint 配置。

```powershell
& F:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe F:/Utorrent/ZCase/ZCase.uproject '-run=ZCaseEditor.ZCBokoblinSetup' -unattended -nop4 -NullRHI
```

## 手动测试清单

在 `TestLevel` 中用 PIE 依次检查：

其中巡逻步骤仅适用于已接上 Patrol 分支的行为树；之前检查到的当前资产无目标分支为空，不能仅凭存在巡逻 C++ 类就认为会执行巡逻。

1. 出生后在 NavMesh 内随机巡逻，基础动作随速度在 Idle、Walk、Run 间切换。
2. 玩家进入正面视野后立即中断巡逻并追击。
3. 追击期间朝向玩家，进入约 130 cm 攻击距离后停步。
4. 右拳攻击 Montage 播放；玩家只在 Damage NotifyState 的 0.80–0.92 秒窗口内受伤。
5. 玩家留在近距离时，Montage 完成及短暂 Wait 后再次攻击。
6. 玩家后退到攻击距离外时恢复追击。
7. 玩家离开约 2200 cm 丢失视野后，TargetActor 与 Focus 被清除并恢复巡逻。
8. 玩家攻击 Bokoblin 时，现有 HitReact 正常播放，当前攻击与伤害窗口被中断。
9. 生命值归零时播放现有 Death Montage。
10. 死亡后 Brain、移动、Focus、感知和 Combat 均停止，不能再次攻击。
11. 同时观察至少两个 Bokoblin，确认各自目标、攻击和死亡状态互不影响。

若第 1 项没有移动，先在编辑器中按 `P` 确认绿色 NavMesh 覆盖敌人活动区域，再执行 Build → Build Paths 并保存 `TestLevel`。把具体失败步骤、画面表现和 Output Log 发回来即可继续修正。

## 2026-09-05：只在贴近时攻击的排查

根因是 TestLevel 保存的静态导航数据没有覆盖 Bokoblin 所在地面，导致追击无法生成路径；Sight 已经正常发现玩家。

- 修复前：玩家与第一只敌人相距约 613 cm，三只敌人的 TargetActor 均为玩家，速度为 0、MoveStatus 为 Idle，实际导航查询返回无效路径。
- 修复：在当前编辑器执行 `RebuildNavigation`，随后保存 TestLevel。没有改动 AI C++、行为树或动画资产。
- 修复后：相同出生位置下，三只敌人的路径均有效且完整；第一只敌人移动约 504 cm，在距离玩家约 109 cm 处停下。运行时读取到了攻击 Montage。此检查证明追击与攻击动画启动，未证明拳头命中及扣血正确。
- 按用户选择保留正面视野追击，不增加固定领地警戒或返回出生点逻辑。

Rider 工程文件已重新生成，新增 AI、Enemy、AnimInstance 和目标指示器文件已进入 `ZCase.vcxproj` 及 `.filters`。下次新增代码后可运行：

```powershell
& F:/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe -projectfiles -project=F:/Utorrent/ZCase/ZCase.uproject -game -rocket
```

## 2026-09-12：出生点距离限制与返程

Controller 接管敌人时记录初始位置为出生点；返程及重新索敌不会更改这个位置。每 0.2 秒检查敌人到出生点的水平直线距离，超出范围便停止行为树、清除目标和 Focus、取消当前攻击，使用 ChaseSpeed 沿 NavMesh 跑回。

在 BP_Enemy_Bokoblin 的 Class Defaults 或关卡敌人实例的 `ZCase | Bokoblin | AI` 分类中调整：

- `MaxChaseDistance`：默认 2000 cm，限制敌人与出生点的距离，不是敌人与玩家的距离或累计路程。
- `ReturnAcceptanceRadius`：默认 75 cm，回家水平容差；运行时不超过最大活动距离的一半。另需落地且高度差不超过 75 cm，避免在其他楼层误判到家。

返程期间忽略新的索敌和攻击请求；受击、死亡仍走原有系统，不回血、不无敌。受击恢复后继续返程。路径失败最多每秒重试，不瞬移，也不会在半路恢复战斗。到家后恢复原行为树并重新查询当前可见玩家；保留 Sight 发现条件，不添加全方向领地感知。

无需更改 BB/BT 或放置额外出生点 Actor。需保证出生点与返程路线有可用导航。保存工作并重启编辑器加载新 C++ 后，手动验证：先把某只敌人的 MaxChaseDistance 调小到 500 cm 便于测试，引它走出范围，观察中断追击并跑回；返程途中靠近不应再次出拳，到家后正面可见玩家应能重新索敌；受击后应继续返程，死亡后不能继续移动。多只敌人应分别返回各自初始位置。

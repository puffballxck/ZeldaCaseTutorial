# 三心生命值 HUD

采用 UMG 图片切换和柔光叠加，不需要 UI 材质。使用 `/Game/Assets/Icon/New_Heart` 下的 `heart_full`、`heart_half`、`heart_empty`。

## 行为

- 固定三颗心，共六个半心；从右向左扣除。
- 玩家每次通过 `TakeDamage` 接收到有效正伤害，扣除最大生命值的六分之一。伤害数值大小不再改变玩家本次扣心数量；敌人的规则不变。
- 第六次命中扣完最后半心后走现有死亡流程。零伤害、负伤害、死亡后的伤害不扣心。
- HUD 监听现有 `UZCAttributeComponent::OnHealthChanged`，不另存游戏生命值。
- 受损的半心立即切换图片，在对应位置播放约 0.4 秒白色柔光、轻微放大和上移。连续受伤的半心各自播放。
- HUD 独立于技能菜单的 WidgetSwitcher；游戏、菜单、死亡状态都保留三颗心的位置。
- 换角色或退出时解除血量委托并移除旧 HUD。

## 编辑器调整

`/Game/_Game/Blueprints/UIs/WBP_HeartHealth` 继承 `ZCHeartHealthWidget`，在 **Class Defaults / ZCase / UI / Health** 调整：

| 参数 | 默认值 | 作用 |
|---|---|---|
| Heart Size | 56 × 52 | 单颗心的逻辑尺寸 |
| Heart Spacing | 6 | 心形之间的间距 |
| Heart Bar Padding | 5 × 10 | 为柔光预留的内边距 |
| Flash Duration | 0.4 秒 | 柔光持续时间 |
| Flash Rise Distance | 8 | 柔光上移距离 |
| Flash Scale Amount | 0.06 | 柔光放大幅度 |

控件树由原生 UMG 在运行时创建，图片无需手工排版。三张心形图片裁掉透明外边距后绘制，不修改源纹理。

`PC_InGame` 的 **ZCase / Player Presentation / Health**：

- `Heart Health Widget Class` 已配置为 `WBP_HeartHealth`。
- `Heart Health Margin` 默认 `(40, 40)`，控制左上角位置，使用 DPI 缩放前的逻辑单位。

修改类默认值后编译、保存并重新进入 PIE。原生控件在创建时读取布局参数。

## 验证

- 编辑器构建：`Build.bat ZCaseEditor Win64 Development F:/Utorrent/ZCase/ZCase.uproject -WaitMutex -NoHotReloadFromIDE`。
- `ZCase.Player.DamageLifecycle` 覆盖有效伤害、六次扣血和死亡、HUD 同步、重复绑定、换绑和解除绑定。
- `ZCase.Enemy.DamageLifecycle` 检查原有敌人伤害规则。
- PIE 已读取 `6 → 5 → 4 → 3 → 2 → 1 → 0` 的半心序列；通过截图检查了局部白光与菜单中常显。
- 动效时长根据静态参考图选取，仍可按观感调整；未进行打包平台验收。

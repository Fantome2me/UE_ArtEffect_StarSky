# UE_ArtEffect_StarSky
RayMatching StarSky

Unreal Engine 5.8 工程：把第三人称模板关卡 `Lvl_ThirdPerson` 的天空替换为
**动态星空 / 星云 / 极光**（StarNest 分形星云 + 点状恒星 + 多层极光帘）。

---

## 实现方式

计算着色器 → Cube RenderTarget → Sky 材质采样的方案（每 texel ~700 次迭代，
但只在 12 Hz 下烘焙，比逐像素便宜约 6.5 倍）：

```
UAuroraSkySubsystem (12 Hz 节流)
  → FAuroraSkyCS 全局计算着色器（一次 dispatch 画完 6 个 cube 面）
  → UTextureRenderTargetCube 512×512×6 PF_FloatRGBA
  → M_AuroraSky（Unlit + bIsSky）采样 cube
  → AAuroraSkyDome（1 km 穹顶） + SkyLight Real Time Capture
```

天空参数（星云/极光/星星的系数与配色）**全部是编译期常量**，
着色器运行时只有 2 个 uniform（`CubeSize` + `SkyTime`）。

---

## 目录

| 路径 | 内容 |
|---|---|
| `Shaders/Private/AuroraSky.usf` | HLSL 主体（所有可调常量都在文件顶部） |
| `Source/StarSkyShaders/` | 全局着色器模块（`LoadingPhase = PostConfigInit`） |
| `Source/UE_ArtEffect_StarSky/AuroraSky/` | 世界子系统 + 穹顶 Actor + 运行时设置 |
| `Scripts/` | 一键落地 / 验证 / 诊断脚本（UE Editor Python） |
| `Docs/星空极光天空_实现说明.md` | **实现说明：架构、参数、踩坑、已知遗留** |

---

## 打开方式

1. 用 **UE 5.8** 打开 `UE_ArtEffect_StarSky.uproject`
2. 打开 `/Game/ThirdPerson/Lvl_ThirdPerson`
3. 编辑器视口里直接就能看到动态星空（不需要 PIE）

想改观感：改 `Shaders/Private/AuroraSky.usf` 顶部的常量后重启编辑器。

> 需要 C++ 编译：`Build.bat UE_ArtEffect_StarSkyEditor Win64 Development -Project="<工程路径>"`
> （编译前必须先关闭编辑器，Live Coding 会锁住 DLL）

---

## 关于模板角色资源（未入库）

`Content/Characters/Mannequins` 是 **UE5「Third Person」模板自带**的 Manny/Quinn 角色资产
（约 88 MB，含骨骼、网格、贴图、动画），**不属于本项目的成果，因此没有上传**，并已加入 `.gitignore`。

clone 下来后如果角色不显示，用同版本引擎（UE 5.8）新建一个「Third Person」模板工程，
把它 `Content/Characters/Mannequins` 整个目录拷到本工程的 `Content/Characters/` 下即可。

> 星空本身的全部内容都完整在库：C++ 模块、`AuroraSky.usf`、关卡与天空材质、
> 世界分区 actor、脚本与文档。

---

## 实现说明

详细文档：**[星空极光天空_实现说明.md](Docs/%E6%98%9F%E7%A9%BA%E6%9E%81%E5%85%89%E5%A4%A9%E7%A9%BA_%E5%AE%9E%E7%8E%B0%E8%AF%B4%E6%98%8E.md)**

包含：UE 5.8 的 `MD_Sky` 移除与 `bIsSky` 替代、`PostConfigInit` 模块要求、
「天空只有半边」的高度雾根因、性能常量化改造、以及与原始方案文档的全部偏差。

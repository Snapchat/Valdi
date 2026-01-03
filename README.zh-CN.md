# Valdi

阅读其他语言: [English](./README.md)

> 状态：Beta（详见英文版 README）

Valdi 是一个跨平台 UI 框架，使用声明式 TypeScript 编写组件，并直接编译为 iOS、Android 和 macOS 的原生视图，不依赖 WebView 或传统 JS Bridge，既保证原生性能又提升开发效率。

## 快速示例

```tsx
import { Component } from 'valdi_core/src/Component';

class HelloWorld extends Component {
  onRender() {
    const message = 'Hello World! 👻';
    <view backgroundColor='#FFFC00' padding={30}>
      <label color='black' value={message} />
    </view>;
  }
}
```

## 快速链接

- 安装与上手: `./docs/INSTALL.md`
- 文档总览: `./docs/README.md`
- Codelabs: `./docs/docs/start-code-lab.md`
- API 速查: `./docs/api/api-quick-reference.md`
- 常见问题: `./docs/docs/faq.md`
- 组件库: `https://github.com/Snapchat/Valdi_Widgets`

## 为什么选择 Valdi

- 原生性能：声明式 TSX 组件直接编译为平台视图
- 开发效率：毫秒级热重载、VSCode 端到端调试
- 灵活集成：可嵌入原生或在 Valdi 中嵌入原生视图
- 深度原生绑定：自动生成跨语言类型安全绑定，支持 Protobuf、RxJS 等

## 贡献指南

欢迎贡献与修订，请务必遵循 `./CONTRIBUTING.md` 的指南与流程（包括代码风格、提交规范、CLA 等）。中文 README 仅提供概览，详细与最新信息以英文文档为准。

## 社区与支持

- Discord: `https://discord.gg/uJyNEeYX2U`

## 许可协议

MIT 许可证：`./LICENSE.md`

—— 最后更新：2025-11-17（以英文版为准）


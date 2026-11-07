// ── 站点主题 —— 全站亮/暗换肤单源 ──
// 根 ThemeProvider 读 siteTheme()（main.js），壳与文稿层颜色写 @token；
// 切换写 themeState → 整树 reconcile → Provider 原位复用 + setTheme →
// 子树 token 按新主题重解析（框架运行时换主题通路，不重建节点、不丢
// 滚动/焦点/输入态）。主题对象为模块级常量：同一 opaque 跨轮复用，
// 非切换的 reconcile 判等零动作。
import { State, theme } from 'kwikui';

// 亮色：色值取站点既有 slate/蓝系，token 化后亮色观感基本不变
export const lightTheme = theme({
    mode: 'light',
    colors: {
        primary: '#2563EB', onPrimary: '#FFFFFF',
        surface: '#FFFFFF', onSurface: '#0F172A',
        surfaceVariant: '#F5F7FB', onSurfaceVariant: '#64748B',
        error: '#DC2626', onError: '#FFFFFF',
        outline: '#CBD5E1', divider: '#E2E8F0',
        disabled: '#F1F5F9', disabledText: '#94A3B8',
    },
    shape: { borderRadius: 8 },
});

// 暗色：页面底（surfaceVariant）最深、卡片/栏（surface）次之，层次与亮色镜像
export const darkTheme = theme({
    mode: 'dark',
    colors: {
        primary: '#60A5FA', onPrimary: '#0B1120',
        surface: '#111827', onSurface: '#E5E7EB',
        surfaceVariant: '#0B1120', onSurfaceVariant: '#9CA3AF',
        error: '#F87171', onError: '#0B1120',
        outline: '#374151', divider: '#273449',
        disabled: '#1F2937', disabledText: '#6B7280',
    },
    shape: { borderRadius: 8 },
});

// 演示卡样张主题 = 框架默认主题（theme() 无参即 defaultTheme）：演示内容是
// 组件用法教学，色值与示例代码同源——两种站点模式下都按"未设主题"的
// 默认外观呈现（与换肤前逐像素一致），见 page_kit.demoCard
export const specimenTheme = theme();

export const themeState = new State({ mode: 'light' });

export const isDark = () => themeState.mode === 'dark';

/** 当前站点主题（根 ThemeProvider 的 theme 值）*/
export const siteTheme = () => (isDark() ? darkTheme : lightTheme);

/** 亮 ⇄ 暗切换 */
export const toggleTheme = () => {
    themeState.mode = isDark() ? 'light' : 'dark';
};

// 导航选中行底色：12 token 固定集无"主色容器"档，按模式二选一
// （token 集扩容立项前的站点专属色，集中于此不散落）
export const navSelectedBg = () => (isDark() ? '#1E3A5F' : '#E8EFFC');

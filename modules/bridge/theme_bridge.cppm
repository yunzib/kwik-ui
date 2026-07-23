module;
#include "quickjs.h"
export module kwik.bridge.theme_bridge;
import kwik.core.theme;
import kwik.core.types;

export ThemeData parseTheme(JSContext* ctx, JSValue val);
export JSValue wrapThemeData(JSContext* ctx, const ThemeData& data);
export const ThemeData* unwrapThemeData(JSValue obj);
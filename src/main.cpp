// blurcap — Hyprland plugin: в захвате экрана (screencast / screen-share) окна,
// помеченные нативным правилом `no_screen_share = true`, не зачёрниваются, а
// РАЗМЫВАЮТСЯ. На реальном мониторе окно видно нормально.
//
// Как это работает:
//   Hyprland уже умеет прятать окна из захвата (правило no_screen_share) —
//   в отдельном проходе CScreenshareFrame::renderMonitor() он рисует mirror-
//   текстуру монитора в буфер захвата, а поверх noScreenShare-окон заливает
//   чёрные прямоугольники через g_pHyprRenderer->draw(SRectData{.color=BLACK}).
//
//   Мы вешаем два function-hook'а:
//     1) renderMonitor — оборачиваем: выставляем флаг "идёт захват" на время вызова.
//     2) draw(SRectData) — если идёт захват и прямоугольник чёрный непрозрачный,
//        подменяем его на blur=true с прозрачным цветом. Тот же прямоугольник
//        теперь размывает уже отрисованную под ним mirror-текстуру окна.
//
//   Скриншоты (grim) не трогаются — они не идут через renderMonitor с damage,
//   да и одиночный кадр пользователь контролирует сам. Цель — видео-захват.

#define WLR_USE_UNSTABLE

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/version.h>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/helpers/Color.hpp>

inline HANDLE         PHANDLE              = nullptr;
inline CFunctionHook* g_pRenderMonitorHook = nullptr;
inline CFunctionHook* g_pDrawRectHook      = nullptr;

// Выставлен, пока выполняется CScreenshareFrame::renderMonitor() — т.е. идёт
// рендер кадра захвата экрана.
inline bool g_inCapture = false;

// Типы оригиналов для вызова через трамплин.
typedef void (*tRenderMonitor)(void* thisptr);
typedef void (*tDrawRect)(void* thisptr, const CRectPassElement::SRectData& data, const CRegion& damage);

static bool isBlackOpaque(const CHyprColor& c) {
    return c.r < 0.01 && c.g < 0.01 && c.b < 0.01 && c.a > 0.99;
}

// --- hooks -------------------------------------------------------------------

static void hkRenderMonitor(void* thisptr) {
    g_inCapture = true;
    (*(tRenderMonitor)g_pRenderMonitorHook->m_original)(thisptr);
    g_inCapture = false;
}

static void hkDrawRect(void* thisptr, const CRectPassElement::SRectData& data, const CRegion& damage) {
    // Чёрная непрозрачная заливка во время захвата = noScreenShare-окно.
    // Превращаем её в блюр того, что уже нарисовано под ней (кадр окна).
    if (g_inCapture && !data.blur && isBlackOpaque(data.color)) {
        CRectPassElement::SRectData mod = data;
        mod.blur  = true;
        mod.blurA = 1.0f;
        mod.color = CHyprColor{0.0, 0.0, 0.0, 0.0}; // прозрачный — остаётся только блюр
        (*(tDrawRect)g_pDrawRectHook->m_original)(thisptr, mod, damage);
        return;
    }

    (*(tDrawRect)g_pDrawRectHook->m_original)(thisptr, data, damage);
}

// --- plugin lifecycle --------------------------------------------------------

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();
    if (HASH != __hyprland_api_get_client_hash()) {
        HyprlandAPI::addNotification(PHANDLE,
            "[blurcap] несовпадение версии Hyprland — плагин не загружен",
            CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[blurcap] version mismatch");
    }

    // Hook 1: CScreenshareFrame::renderMonitor — флаг "идёт захват".
    {
        auto fns = HyprlandAPI::findFunctionsByName(PHANDLE, "renderMonitor");
        for (auto& f : fns) {
            if (f.demangled.contains("CScreenshareFrame::renderMonitor(")) {
                g_pRenderMonitorHook = HyprlandAPI::createFunctionHook(PHANDLE, f.address, (void*)&hkRenderMonitor);
                break;
            }
        }
    }

    // Hook 2: draw(const CRectPassElement::SRectData&, const CRegion&).
    {
        auto fns = HyprlandAPI::findFunctionsByName(PHANDLE, "draw");
        for (auto& f : fns) {
            if (f.demangled.contains("SRectData")) {
                g_pDrawRectHook = HyprlandAPI::createFunctionHook(PHANDLE, f.address, (void*)&hkDrawRect);
                break;
            }
        }
    }

    if (!g_pRenderMonitorHook || !g_pDrawRectHook) {
        HyprlandAPI::addNotification(PHANDLE,
            "[blurcap] не нашёл функции для хука (renderMonitor/draw) — несовместимая версия?",
            CHyprColor{1.0, 0.2, 0.2, 1.0}, 7000);
        throw std::runtime_error("[blurcap] hook target not found");
    }

    g_pRenderMonitorHook->hook();
    g_pDrawRectHook->hook();

    HyprlandAPI::addNotification(PHANDLE,
        "[blurcap] загружен — окна с no_screen_share будут размыты в захвате",
        CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"blurcap", "Blur no_screen_share windows in screen capture", "dmr", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_pRenderMonitorHook)
        HyprlandAPI::removeFunctionHook(PHANDLE, g_pRenderMonitorHook);
    if (g_pDrawRectHook)
        HyprlandAPI::removeFunctionHook(PHANDLE, g_pDrawRectHook);
}

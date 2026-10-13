// In-game HUD drawn with ImGui draw lists over the game viewport.
#include "FPSGame.h"
#include "GameComponents.h"
#include "GameSystems.h"

#include <imgui.h>

#include <cmath>

namespace FPS {

using namespace Forge;

namespace {
void textShadow(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text) {
    dl->AddText(font, size, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, (color >> 24) * 160 / 255), text);
    dl->AddText(font, size, pos, color, text);
}

ImVec2 measure(ImFont* font, float size, const char* text) { return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text); }

void centered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text) {
    ImVec2 s = measure(font, size, text);
    textShadow(dl, font, size, ImVec2(center.x - s.x * 0.5f, center.y - s.y * 0.5f), color, text);
}

ImU32 alpha(ImU32 color, float a) {
    int base = (int)((color >> 24) & 0xFF);
    int na = (int)(base * std::fmax(0.0f, std::fmin(a, 1.0f)));
    return (color & 0x00FFFFFF) | ((ImU32)na << 24);
}
} // namespace

void drawHud(Scene& scene, const Viewport& vp) {
    ImDrawList* dl = vp.drawList ? vp.drawList : ImGui::GetForegroundDrawList();
    const Fonts& fonts = Application::get().fonts();
    float scale = glm::clamp(vp.size.y / 900.0f, 0.75f, 1.6f);
    ImVec2 min(vp.position.x, vp.position.y);
    ImVec2 max(vp.position.x + vp.size.x, vp.position.y + vp.size.y);
    ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float pad = 28.0f * scale;
    float medium = fonts.medium->FontSize * scale;
    float large = fonts.large->FontSize * scale;
    float small = 20.0f * scale;

    Entity pe = findPlayer(scene);
    GameManager* gm = findGameManager(scene);
    if (!scene.valid(pe)) return;
    Player& player = scene.get<Player>(pe);
    Health* hp = scene.tryGet<Health>(pe);
    Weapon* weapon = scene.valid(player.weapon) ? scene.tryGet<Weapon>(player.weapon) : nullptr;

    // Damage vignette
    if (player.damageFlash > 0.0f) {
        float a = std::fmin(player.damageFlash, 1.0f) * 0.55f;
        ImU32 red = IM_COL32(200, 0, 0, (int)(a * 255)), clear = IM_COL32(200, 0, 0, 0);
        float w = vp.size.x * 0.22f, h = vp.size.y * 0.22f;
        dl->AddRectFilledMultiColor(min, ImVec2(min.x + w, max.y), red, clear, clear, red);
        dl->AddRectFilledMultiColor(ImVec2(max.x - w, min.y), max, clear, red, red, clear);
        dl->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + h), red, red, clear, clear);
        dl->AddRectFilledMultiColor(ImVec2(min.x, max.y - h), max, clear, clear, red, red);
    }

    // Crosshair (gap widens with spread)
    if (!player.dead) {
        float spread = weapon ? weapon->spread + weapon->bloom : 1.0f;
        float gap = (5.0f + spread * 6.0f) * scale, len = 9.0f * scale, th = 2.0f * scale;
        ImU32 col = IM_COL32(255, 255, 255, 230), shadow = IM_COL32(0, 0, 0, 150);
        for (int pass = 0; pass < 2; ++pass) {
            ImU32 cc = pass == 0 ? shadow : col;
            float o = pass == 0 ? 1.0f : 0.0f, t = pass == 0 ? th + 2.0f : th;
            dl->AddLine(ImVec2(c.x + o, c.y - gap - len), ImVec2(c.x + o, c.y - gap), cc, t);
            dl->AddLine(ImVec2(c.x + o, c.y + gap), ImVec2(c.x + o, c.y + gap + len), cc, t);
            dl->AddLine(ImVec2(c.x - gap - len, c.y + o), ImVec2(c.x - gap, c.y + o), cc, t);
            dl->AddLine(ImVec2(c.x + gap, c.y + o), ImVec2(c.x + gap + len, c.y + o), cc, t);
        }
        dl->AddCircleFilled(c, 1.6f * scale, col);
        if (gm && gm->hitMarker > 0.0f) {
            float s = 7.0f * scale, g = 4.0f * scale;
            ImU32 hit = alpha(IM_COL32(255, 80, 60, 255), gm->hitMarker / 0.15f);
            dl->AddLine(ImVec2(c.x - g - s, c.y - g - s), ImVec2(c.x - g, c.y - g), hit, 2.5f * scale);
            dl->AddLine(ImVec2(c.x + g + s, c.y - g - s), ImVec2(c.x + g, c.y - g), hit, 2.5f * scale);
            dl->AddLine(ImVec2(c.x - g - s, c.y + g + s), ImVec2(c.x - g, c.y + g), hit, 2.5f * scale);
            dl->AddLine(ImVec2(c.x + g + s, c.y + g + s), ImVec2(c.x + g, c.y + g), hit, 2.5f * scale);
        }
    }

    // Health (bottom-left)
    if (hp) {
        float frac = glm::clamp(hp->current / std::fmax(hp->max, 1.0f), 0.0f, 1.0f);
        float w = 300.0f * scale, h = 16.0f * scale;
        ImVec2 p(min.x + pad, max.y - pad - h);
        dl->AddRectFilled(ImVec2(p.x - 4, p.y - medium - 8 * scale), ImVec2(p.x + w + 4, p.y + h + 4), IM_COL32(0, 0, 0, 90), 6.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", (int)std::ceil(hp->current));
        textShadow(dl, fonts.medium, medium, ImVec2(p.x, p.y - medium - 4 * scale), IM_COL32(255, 255, 255, 255), buf);
        textShadow(dl, fonts.regular, small, ImVec2(p.x + measure(fonts.medium, medium, buf).x + 8 * scale, p.y - small - 6 * scale),
                   IM_COL32(200, 200, 200, 255), "HEALTH");
        ImU32 fill = frac > 0.5f ? IM_COL32(90, 220, 120, 255) : (frac > 0.25f ? IM_COL32(240, 190, 60, 255) : IM_COL32(235, 70, 60, 255));
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(30, 30, 30, 200), 3.0f);
        dl->AddRectFilled(p, ImVec2(p.x + w * frac, p.y + h), fill, 3.0f);
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(0, 0, 0, 200), 3.0f);
    }

    // Ammo (bottom-right)
    if (weapon) {
        char ammo[32], reserve[32];
        std::snprintf(ammo, sizeof(ammo), "%d", weapon->ammo);
        std::snprintf(reserve, sizeof(reserve), "/ %d", weapon->reserveAmmo);
        ImVec2 as = measure(fonts.large, large, ammo), rs = measure(fonts.medium, medium, reserve);
        ImVec2 p(max.x - pad - as.x - rs.x - 8 * scale, max.y - pad - as.y);
        dl->AddRectFilled(ImVec2(p.x - 10, p.y - 4), ImVec2(max.x - pad + 8, max.y - pad + 6), IM_COL32(0, 0, 0, 90), 6.0f);
        ImU32 ammoCol = weapon->ammo == 0 ? IM_COL32(235, 70, 60, 255) : IM_COL32(255, 255, 255, 255);
        textShadow(dl, fonts.large, large, p, ammoCol, ammo);
        textShadow(dl, fonts.medium, medium, ImVec2(p.x + as.x + 8 * scale, p.y + as.y - rs.y - 6 * scale), IM_COL32(190, 190, 190, 255), reserve);
        const char* status = nullptr;
        if (weapon->reloading) status = "RELOADING...";
        else if (weapon->ammo == 0 && weapon->reserveAmmo > 0) status = "PRESS R TO RELOAD";
        else if (weapon->ammo == 0) status = "OUT OF AMMO";
        if (status) {
            float blink = weapon->reloading ? 1.0f : 0.6f + 0.4f * std::sin((float)Time::time() * 8.0f);
            centered(dl, fonts.bold, small * 1.1f, ImVec2(c.x, c.y + 60.0f * scale), alpha(IM_COL32(255, 220, 120, 255), blink), status);
        }
        if (weapon->reloading) {
            float prog = 1.0f - weapon->reloadTimer / std::fmax(weapon->reloadTime, 0.01f);
            ImVec2 bp(c.x - 60 * scale, c.y + 78 * scale);
            dl->AddRectFilled(bp, ImVec2(bp.x + 120 * scale, bp.y + 4 * scale), IM_COL32(0, 0, 0, 140));
            dl->AddRectFilled(bp, ImVec2(bp.x + 120 * scale * prog, bp.y + 4 * scale), IM_COL32(255, 220, 120, 255));
        }
    }

    if (gm) {
        // Wave / enemies (top-center) and score (top-right)
        char wave[64];
        int alive = (int)scene.registry().count<Enemy>();
        std::snprintf(wave, sizeof(wave), "WAVE %d", gm->wave);
        centered(dl, fonts.medium, medium, ImVec2(c.x, min.y + pad + medium * 0.5f), IM_COL32(255, 255, 255, 255), wave);
        char enemies[64];
        std::snprintf(enemies, sizeof(enemies), "%d drone%s remaining", alive + gm->toSpawn, alive + gm->toSpawn == 1 ? "" : "s");
        centered(dl, fonts.regular, small, ImVec2(c.x, min.y + pad + medium + 8 * scale), IM_COL32(210, 210, 210, 230), enemies);

        char score[64];
        std::snprintf(score, sizeof(score), "%06d", gm->score);
        ImVec2 ss = measure(fonts.medium, medium, score);
        textShadow(dl, fonts.medium, medium, ImVec2(max.x - pad - ss.x, min.y + pad), IM_COL32(255, 230, 140, 255), score);
        textShadow(dl, fonts.regular, small, ImVec2(max.x - pad - ss.x, min.y + pad + ss.y), IM_COL32(200, 200, 200, 220), "SCORE");

        char kills[32];
        std::snprintf(kills, sizeof(kills), "KILLS %d", gm->kills);
        textShadow(dl, fonts.regular, small, ImVec2(min.x + pad, min.y + pad), IM_COL32(220, 220, 220, 230), kills);

        // Big center message (wave announcements)
        if (gm->messageTimer > 0.0f && !gm->message.empty() && !gm->gameOver) {
            float a = std::fmin(gm->messageTimer / 0.5f, 1.0f);
            centered(dl, fonts.large, large * 1.2f, ImVec2(c.x, min.y + vp.size.y * 0.3f), alpha(IM_COL32(255, 255, 255, 255), a), gm->message.c_str());
        }
        // Pickup toast
        if (gm->toastTimer > 0.0f && !gm->toast.empty()) {
            float a = std::fmin(gm->toastTimer / 0.4f, 1.0f);
            centered(dl, fonts.bold, small * 1.2f, ImVec2(c.x, c.y + 110.0f * scale), alpha(IM_COL32(140, 255, 160, 255), a), gm->toast.c_str());
        }

        // Game over screen
        if (gm->gameOver) {
            float a = std::fmin(gm->gameOverTimer / 1.0f, 1.0f);
            dl->AddRectFilled(min, max, IM_COL32(40, 0, 0, (int)(170 * a)));
            centered(dl, fonts.large, large * 1.4f, ImVec2(c.x, c.y - 60 * scale), alpha(IM_COL32(255, 80, 70, 255), a), "YOU DIED");
            char stats[96];
            std::snprintf(stats, sizeof(stats), "Wave %d   -   %d kills   -   Score %d", gm->wave, gm->kills, gm->score);
            centered(dl, fonts.medium, medium, ImVec2(c.x, c.y + 10 * scale), alpha(IM_COL32(255, 255, 255, 255), a), stats);
            centered(dl, fonts.regular, small * 1.1f, ImVec2(c.x, c.y + 60 * scale), alpha(IM_COL32(220, 220, 220, 255), a),
                     "Press R or click to restart");
        }
    }
}

} // namespace FPS

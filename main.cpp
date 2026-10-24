#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

constexpr int   kWidth       = 1280;
constexpr int   kHeight      = 720;
constexpr float kPaddleW     = 12.0f;
constexpr float kPaddleH     = 90.0f;
constexpr float kPaddleSpeed = 420.0f;
constexpr float kBallSize    = 12.0f;
constexpr float kBallStart   = 360.0f;
constexpr float kBallMax     = 800.0f;
constexpr int   kWinScore    = 7;

struct Ball {
    Vector2 pos;
    Vector2 vel;
};

enum class State { Menu, Playing, Paused, GameOver };

static void ResetBall(Ball &ball, int direction) {
    ball.pos = {kWidth / 2.0f, kHeight / 2.0f};
    float angle = (float)GetRandomValue(-30, 30) * DEG2RAD;
    ball.vel = {direction * kBallStart * std::cos(angle), kBallStart * std::sin(angle)};
}

// Erzeugt einen Rechteck-Ton mit linearem Frequenzverlauf (Hz) und Ausklang.
static Sound MakeTone(float freqStart, float freqEnd, float seconds, float volume = 0.4f) {
    constexpr int kRate = 44100;
    int frames = (int)(seconds * kRate);
    short *samples = (short *)RL_MALLOC(frames * sizeof(short));
    float phase = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        phase += (freqStart + (freqEnd - freqStart) * t) / kRate;
        phase -= std::floor(phase);
        float env = 1.0f - t;
        samples[i] = (short)((phase < 0.5f ? 1.0f : -1.0f) * env * volume * 32767);
    }
    Wave wave {(unsigned)frames, kRate, 16, 1, samples};
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

// Plasma wird in niedriger Auflösung berechnet und per bilinearer Filterung hochskaliert.
constexpr int kPlasmaW = 80;
constexpr int kPlasmaH = 60;

static void UpdatePlasma(Color *pixels, float t) {
    for (int y = 0; y < kPlasmaH; y++) {
        for (int x = 0; x < kPlasmaW; x++) {
            float fx = x * 0.12f, fy = y * 0.12f;
            float v = std::sin(fx + t)
                    + std::sin(fy * 1.3f - t * 0.7f)
                    + std::sin((fx + fy) * 0.7f + t * 0.5f)
                    + std::sin(std::hypot(fx - 4.5f, fy - 3.5f) * 1.1f - t * 0.9f);
            v = v * 0.125f + 0.5f; // 0..1

            // Dunkle Basis mit nur geringer Farbschwankung -> wenig Kontrast
            pixels[y * kPlasmaW + x] = {
                (unsigned char)(10 + 22 * (0.5f + 0.5f * std::sin(v * 6.28f))),
                (unsigned char)(8  + 16 * (0.5f + 0.5f * std::sin(v * 6.28f + 2.1f))),
                (unsigned char)(22 + 30 * (0.5f + 0.5f * std::sin(v * 6.28f + 4.2f))),
                255};
        }
    }
}

// Echter Vollbildmodus (verdeckt auch Panels wie die KDE-Leisten). raylib fordert beim Umschalten
// die aktuelle Fenstergröße als Auflösung an, daher vorher auf die Monitorgröße setzen.
static void ToggleFullscreenMode() {
    if (!IsWindowFullscreen()) {
        int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
    } else {
        ToggleFullscreen();
        SetWindowSize(kWidth, kHeight);
    }
}

// Rekord (längster Ballwechsel) liegt neben der Programmdatei.
static std::string HighscorePath() {
    return std::string(GetApplicationDirectory()) + "highscore.txt";
}

static int LoadHighscore() {
    std::string path = HighscorePath();
    if (!FileExists(path.c_str())) return 0;
    char *text = LoadFileText(path.c_str());
    int value = text ? std::atoi(text) : 0;
    UnloadFileText(text);
    return value > 0 ? value : 0;
}

static void SaveHighscore(int value) {
    std::string text = std::to_string(value);
    SaveFileText(HighscorePath().c_str(), text.data());
}

int main() {
    InitWindow(kWidth, kHeight, "Pong");
    InitAudioDevice();
    SetTargetFPS(60);

    Color *plasmaPixels = (Color *)RL_MALLOC(kPlasmaW * kPlasmaH * sizeof(Color));
    UpdatePlasma(plasmaPixels, 0.0f);
    Image plasmaImg = GenImageColor(kPlasmaW, kPlasmaH, BLACK);
    Texture2D plasmaTex = LoadTextureFromImage(plasmaImg);
    UnloadImage(plasmaImg);
    SetTextureFilter(plasmaTex, TEXTURE_FILTER_BILINEAR);

    // Das Spiel wird immer in 800x600 gezeichnet und für den Vollbildmodus skaliert.
    RenderTexture2D canvas = LoadRenderTexture(kWidth, kHeight);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    SetExitKey(KEY_NULL); // ESC beendet erst nach dem Verlassen des Vollbilds
    bool quit = false;

    Sound sndPaddle = MakeTone(520, 520, 0.08f);
    Sound sndWall   = MakeTone(260, 260, 0.08f);
    Sound sndScore  = MakeTone(400, 100, 0.40f);
    Sound sndWin    = MakeTone(300, 900, 0.70f);
    Sound sndMenu   = MakeTone(700, 900, 0.10f);

    Rectangle left  {30.0f, (kHeight - kPaddleH) / 2, kPaddleW, kPaddleH};
    Rectangle right {kWidth - 30.0f - kPaddleW, (kHeight - kPaddleH) / 2, kPaddleW, kPaddleH};
    Ball ball;
    int scoreL = 0, scoreR = 0;
    bool vsAI = true;
    int rally = 0;                  // Schlägertreffer seit dem letzten Punkt
    int bestRally = LoadHighscore();
    bool highscoreDirty = false;
    State state = State::Menu;
    ResetBall(ball, 1);

    while (!WindowShouldClose() && !quit) {
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {
            ToggleFullscreenMode();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (IsWindowFullscreen()) ToggleFullscreenMode();
            else quit = true;
        }
        float dt = GetFrameTime();

        if (state == State::Menu) {
            if (IsKeyPressed(KEY_ONE))  { vsAI = true;  state = State::Playing; PlaySound(sndMenu); }
            if (IsKeyPressed(KEY_TWO))  { vsAI = false; state = State::Playing; PlaySound(sndMenu); }
        } else if (state == State::Paused) {
            if (IsKeyPressed(KEY_P)) { state = State::Playing; PlaySound(sndMenu); }
        } else if (state == State::Playing) {
            if (IsKeyPressed(KEY_P)) { state = State::Paused; PlaySound(sndMenu); }

            // Linker Schläger: W/S
            float dl = (IsKeyDown(KEY_S) - IsKeyDown(KEY_W)) * kPaddleSpeed * dt;
            left.y += dl;

            // Rechter Schläger: Pfeiltasten oder KI
            if (vsAI) {
                float target = ball.pos.y - kPaddleH / 2;
                float diff = target - right.y;
                float step = kPaddleSpeed * 0.8f * dt;
                if (ball.vel.x > 0 && std::fabs(diff) > 6.0f)
                    right.y += std::clamp(diff, -step, step);
            } else {
                right.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * kPaddleSpeed * dt;
            }
            left.y  = std::clamp(left.y,  0.0f, kHeight - kPaddleH);
            right.y = std::clamp(right.y, 0.0f, kHeight - kPaddleH);

            ball.pos.x += ball.vel.x * dt;
            ball.pos.y += ball.vel.y * dt;

            // Wände oben/unten
            if (ball.pos.y < 0) { ball.pos.y = 0; ball.vel.y = -ball.vel.y; PlaySound(sndWall); }
            if (ball.pos.y > kHeight - kBallSize) {
                ball.pos.y = kHeight - kBallSize;
                ball.vel.y = -ball.vel.y;
                PlaySound(sndWall);
            }

            // Schläger-Kollision: Abprallwinkel hängt von der Trefferstelle ab
            Rectangle br {ball.pos.x, ball.pos.y, kBallSize, kBallSize};
            for (const Rectangle *p : {&left, &right}) {
                bool isLeft = (p == &left);
                if (CheckCollisionRecs(br, *p) && (ball.vel.x < 0) == isLeft) {
                    float hit = ((ball.pos.y + kBallSize / 2) - (p->y + kPaddleH / 2)) / (kPaddleH / 2);
                    float speed = std::min(std::hypot(ball.vel.x, ball.vel.y) * 1.06f, kBallMax);
                    float angle = hit * 60.0f * DEG2RAD;
                    ball.vel.x = (isLeft ? 1 : -1) * speed * std::cos(angle);
                    ball.vel.y = speed * std::sin(angle);
                    ball.pos.x = isLeft ? p->x + kPaddleW : p->x - kBallSize;
                    PlaySound(sndPaddle);
                    if (++rally > bestRally) { bestRally = rally; highscoreDirty = true; }
                }
            }

            // Punkte
            bool scored = false;
            if (ball.pos.x + kBallSize < 0)  { scoreR++; ResetBall(ball,  1); PlaySound(sndScore); scored = true; }
            if (ball.pos.x > kWidth)         { scoreL++; ResetBall(ball, -1); PlaySound(sndScore); scored = true; }
            if (scored) {
                rally = 0;
                if (highscoreDirty) { SaveHighscore(bestRally); highscoreDirty = false; }
            }
            if (scoreL >= kWinScore || scoreR >= kWinScore) {
                state = State::GameOver;
                PlaySound(sndWin);
            }
        } else if (IsKeyPressed(KEY_ENTER)) {
            scoreL = scoreR = 0;
            left.y = right.y = (kHeight - kPaddleH) / 2;
            ResetBall(ball, 1);
            rally = 0;
            state = State::Menu;
        }

        UpdatePlasma(plasmaPixels, (float)GetTime() * 0.35f);
        UpdateTexture(plasmaTex, plasmaPixels);

        BeginDrawing();
        BeginTextureMode(canvas);
        DrawTexturePro(plasmaTex, {0, 0, (float)kPlasmaW, (float)kPlasmaH},
                       {0, 0, (float)kWidth, (float)kHeight}, {0, 0}, 0.0f, WHITE);
        for (int y = 0; y < kHeight; y += 30) DrawRectangle(kWidth / 2 - 2, y, 4, 16, DARKGRAY);

        if (state == State::Menu) {
            DrawText("PONG", kWidth / 2 - MeasureText("PONG", 80) / 2, 140, 80, WHITE);
            DrawText("1 - Gegen Computer", kWidth / 2 - MeasureText("1 - Gegen Computer", 28) / 2, 300, 28, LIGHTGRAY);
            DrawText("2 - Zwei Spieler", kWidth / 2 - MeasureText("2 - Zwei Spieler", 28) / 2, 340, 28, LIGHTGRAY);
            DrawText("Links: W/S   Rechts: Pfeil hoch/runter", kWidth / 2 - MeasureText("Links: W/S   Rechts: Pfeil hoch/runter", 20) / 2, 450, 20, GRAY);
            if (bestRally > 0) {
                const char *rec = TextFormat("Rekord: %d Ballwechsel", bestRally);
                DrawText(rec, kWidth / 2 - MeasureText(rec, 24) / 2, 390, 24, YELLOW);
            }
            DrawText("P - Pause", kWidth / 2 - MeasureText("P - Pause", 20) / 2, 510, 20, GRAY);
            DrawText("F11 / Alt+Enter - Vollbild", kWidth / 2 - MeasureText("F11 / Alt+Enter - Vollbild", 20) / 2, 480, 20, GRAY);
        } else {
            DrawRectangleRec(left, WHITE);
            DrawRectangleRec(right, WHITE);
            DrawRectangle((int)ball.pos.x, (int)ball.pos.y, (int)kBallSize, (int)kBallSize, WHITE);
            DrawText(TextFormat("%d", scoreL), kWidth / 4, 20, 60, WHITE);
            DrawText(TextFormat("%d", scoreR), 3 * kWidth / 4, 20, 60, WHITE);
            const char *info = TextFormat("Ballwechsel: %d   Rekord: %d", rally, bestRally);
            bool isRecord = rally > 0 && rally == bestRally;
            DrawText(info, kWidth / 2 - MeasureText(info, 20) / 2, kHeight - 30, 20, isRecord ? YELLOW : LIGHTGRAY);
        }
        if (state == State::Paused) {
            DrawRectangle(0, 0, kWidth, kHeight, {0, 0, 0, 120});
            DrawText("PAUSE", kWidth / 2 - MeasureText("PAUSE", 70) / 2, 230, 70, WHITE);
            DrawText("P - Weiterspielen", kWidth / 2 - MeasureText("P - Weiterspielen", 24) / 2, 320, 24, LIGHTGRAY);
        }
        if (state == State::GameOver) {
            const char *msg = scoreL > scoreR ? "Links gewinnt!" : (vsAI ? "Computer gewinnt!" : "Rechts gewinnt!");
            DrawText(msg, kWidth / 2 - MeasureText(msg, 50) / 2, 240, 50, YELLOW);
            DrawText("Enter - Zurueck zum Menue", kWidth / 2 - MeasureText("Enter - Zurueck zum Menue", 24) / 2, 320, 24, LIGHTGRAY);
        }
        EndTextureMode();

        // Mit Seitenverhältnis (Letterbox) auf das Fenster bzw. den Bildschirm skalieren
        float scale = std::min((float)GetScreenWidth() / kWidth, (float)GetScreenHeight() / kHeight);
        float dw = kWidth * scale, dh = kHeight * scale;
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture, {0, 0, (float)kWidth, -(float)kHeight},
                       {(GetScreenWidth() - dw) / 2, (GetScreenHeight() - dh) / 2, dw, dh},
                       {0, 0}, 0.0f, WHITE);
        EndDrawing();
    }
    if (highscoreDirty) SaveHighscore(bestRally);
    UnloadRenderTexture(canvas);
    UnloadTexture(plasmaTex);
    RL_FREE(plasmaPixels);
    UnloadSound(sndPaddle);
    UnloadSound(sndWall);
    UnloadSound(sndScore);
    UnloadSound(sndWin);
    UnloadSound(sndMenu);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

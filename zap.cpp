// Externals
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <pwd.h>

// Internals
#include "Core/Camera.hpp"
#include "Core/Level.hpp"
#include "Core/LocalPlayer.hpp"
#include "Core/Offsets.hpp"
#include "Core/Player.hpp"

#include "Features/Flickbot.hpp"
#include "Features/Glow.hpp"
#include "Features/Legitbot.hpp"
#include "Features/Misc.hpp"
#include "Features/Radar.hpp"
#include "Features/Ragebot.hpp"
#include "Features/Sense.hpp"
#include "Features/Triggerbot.hpp"
// #include "Features/Test.hpp"

#include "Overlay/FontAwesome.h"
#include "Overlay/GUI.hpp"
#include "Overlay/Overlay.hpp"

#include "Utils/Config.hpp"
#include "Utils/ConfigManager.hpp"
#include "Utils/Features.hpp"
#include "Utils/Memory.hpp"
#include "Utils/XDisplay.hpp"
#include "imgui/imgui.h"

// Objects
XDisplay* X11Display = new XDisplay();
Overlay OverlayWindow = Overlay();
ImDrawList* Canvas;

// Game Objects
Level* Map = new Level();
LocalPlayer* Myself = new LocalPlayer();
Camera* GameCamera = new Camera();

// Players
std::vector<Player*>* HumanPlayers = new std::vector<Player*>;
std::vector<Player*>* Dummies = new std::vector<Player*>;
std::vector<Player*>* Players = new std::vector<Player*>;

// Features
Sense* ESP = new Sense(Map, Players, GameCamera, Myself, X11Display);
Radar* MapRadar = new Radar(X11Display, Players, GameCamera, Map, Myself);
Glow* GlowESP = new Glow(Map, Players, GameCamera, Myself);
Legitbot* Legit = new Legitbot(X11Display, Map, Myself, Players);
Ragebot* Rage = new Ragebot(X11Display, Map, Myself, Players);
Triggerbot* Trigger = new Triggerbot(X11Display, Map, Myself, Players);
Flickbot* Flick = new Flickbot(X11Display, Map, Myself, Players);
Misc* MiscTab = new Misc(X11Display, Map, Myself, Players);
// Other
Overlay* Home = new Overlay;
AdvancedGUI* Advanced = new AdvancedGUI;
Menu* GUI = new Menu(Myself, Advanced);
ConfigManager* Configs = new ConfigManager(Legit, Rage, Flick, Trigger, GlowESP, ESP, MapRadar, MiscTab);

// Booleans and Variables
bool IsMenuOpened = true;
std::chrono::milliseconds LastRead;
std::chrono::milliseconds LastReset;

// Thread
std::atomic_bool StopThread(false);

// Icons
ImFont* IconFont = nullptr;

void Sleep(int Delay) { // Looks better than a bunch of (std::this_thread::sleep_for) lines
    std::this_thread::sleep_for(std::chrono::milliseconds(Delay));
}

// Menu opened?
void MenuStateRun() {
    while (!StopThread) {
        if (InputManager::isKeyDown(Features::Settings::MenuBind)) {
            Features::Home::IsMenuOpened = !Features::Home::IsMenuOpened;
            OverlayWindow.CaptureInput(Features::Home::IsMenuOpened);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// Overlay
bool InitializeOverlayWindow() {
    if (!OverlayWindow.InitializeOverlay()) {
        OverlayWindow.DestroyOverlay();
        return false;
    }
    int ScreenWidth;
    int ScreenHeight;
    OverlayWindow.GetScreenResolution(ScreenWidth, ScreenHeight);
    GameCamera->Initialize(ScreenWidth, ScreenHeight);
    std::cout << "Overlay Initialized!" << std::endl;
    return true;
}

// Load The Default Config
void LoadDefaultConfig() {
    if (std::filesystem::exists("settings.txt")) { // Check if file exists
        Configs->LoadDefaultConfig(); // Loads default config
    }
    else { 
        std::ofstream Default("settings.txt"); // Creates and opens file
        Default << "Blank"; // Writes a default config to use
        Default.close();
        Configs->LoadDefaultConfig(); // Loads default config
    }
}

// Interface
ImVec4 ProcessingTimeColor;

void CreateTabButton(const char* title, const Menu::MenuTabs tab, const ImVec2 size) {
    const ImVec4 BaseTabButton = ImVec4(GUI->DetailColor.x, GUI->DetailColor.y, GUI->DetailColor.z, 0.00f);
    const ImVec4 BaseTabButtonActive = ImVec4(GUI->DetailColor.x, GUI->DetailColor.y, GUI->DetailColor.z, 0.250f);
    ImGui::PushStyleColor(ImGuiCol_Button, (GUI->CurrentTab == tab) ? BaseTabButtonActive : BaseTabButton);
    if (ImGui::Button(title, size)) {
        GUI->CurrentTab = tab;
    }
    ImGui::PopStyleColor(1);
}

void RenderUI() {
    auto io = ImGui::GetIO();
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("##Overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                     ImGuiSliderFlags_AlwaysClamp);
    Canvas = ImGui::GetWindowDrawList();
    MapRadar->RenderDrawings(Canvas, Myself, OverlayWindow);
    ESP->RenderDrawings(Canvas, Legit, Myself, OverlayWindow);
    ESP->RenderWatermark(Canvas, Myself, OverlayWindow);
    ESP->RenderKeybinds(Canvas, Legit, Rage, Trigger, MiscTab, OverlayWindow);
    ESP->RenderTargetInformation(Canvas, Legit, OverlayWindow);
    ImGui::End();

    ImGuiStyle& style = ImGui::GetStyle();
    if (Features::Settings::AntiAliasedLines && !style.AntiAliasedLines) {
        style.AntiAliasedLines = true;
    }
    else if (!Features::Settings::AntiAliasedLines && style.AntiAliasedLines) {
        style.AntiAliasedLines = false;
    }

    if (!Features::Home::IsMenuOpened)
        return;

    ImDrawList* DrawList = ImGui::GetBackgroundDrawList();

    GUI->SetStyle();

    // Window Size
    ImGui::SetNextWindowSizeConstraints(ImVec2(GUI->WindowWidth, GUI->WindowHeight), ImVec2(GUI->WindowWidth, GUI->WindowHeight));
    ImGui::SetNextWindowSize(ImVec2(GUI->WindowWidth, GUI->WindowHeight), ImGuiCond_FirstUseEver);

    // Setup flags and begin window
    if (ImGui::Begin("##CheatGUI", &Features::Home::IsMenuOpened, GUI->WindowFlags)) {
      // Setup "MenuSize" so that we can calculate control positions
        ImVec2 MenuSize = ImGui::GetWindowSize();
        // Render Left Panel
        ImGui::SetCursorPos(ImVec2(10, 32));

        ImVec4* colors = ImGui::GetStyle().Colors;

        colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        if (ImGui::BeginChild("##LeftPanel", ImVec2(GUI->WindowWidth / 6 - 5, GUI->WindowHeight - 43), true, ImGuiWindowFlags_NoScrollbar)) {
            colors[ImGuiCol_Border] = colors[ImGuiCol_Border];
            // Color the background of the tab button brighter if it is the active tab
            const ImVec4 BaseTabButton = ImVec4(GUI->DetailColor.x, GUI->DetailColor.y, GUI->DetailColor.z, 0.00f);
            const ImVec4 BaseTabButtonActive = ImVec4(GUI->DetailColor.x, GUI->DetailColor.y, GUI->DetailColor.z, 0.250f);

            ImGuiStyle& Style = ImGui::GetStyle();
            constexpr int ButtonHeight = 43;
            Style.FrameBorderSize = 1;

            colors[ImGuiCol_ButtonHovered] = BaseTabButtonActive;
            colors[ImGuiCol_ButtonActive] = BaseTabButtonActive;
            colors[ImGuiCol_Button] = BaseTabButton;
            colors[ImGuiCol_Text] = GUI->TextColor;

            CreateTabButton(ICON_FA_CROSSHAIRS "            LEGITBOT", Menu::Legitbot, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_CROSSHAIRS "             RAGEBOT", Menu::Ragebot, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_CROSSHAIRS "            FLICKBOT", Menu::Flickbot, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_CROSSHAIRS "      TRIGGERBOT", Menu::Triggerbot, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_EYE "                      GLOW", Menu::Glow, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_EYE_SLASH "                         ESP", Menu::ESP, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_COG "                        MISC", Menu::Misc, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_COGS "              SETTINGS", Menu::Settings, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));
            CreateTabButton(ICON_FA_SAVE "                CONFIGS", Menu::Config, ImVec2(MenuSize.x / 6 - 35, ButtonHeight));

            ImGui::EndChild();
        }

        std::stringstream TitleText, Date, Time, Version, UnknownCheats, Spacings;
        TitleText << "zap-client";
        Date << OverlayWindow.currentDateTime(2);
        Time << OverlayWindow.currentDateTime(3);
        Version << CheatVersion;
        UnknownCheats << "unknowncheats.me";
        Spacings << "  |  ";
        std::string combined = TitleText.str() + Spacings.str() + Date.str() + Spacings.str() + Time.str() + Spacings.str() + Version.str() + Spacings.str() + UnknownCheats.str();
        const char* combinedText = combined.c_str();

        ImVec2 WindowPosition = ImGui::GetWindowPos();
        int TextPosition = WindowPosition.x + (GUI->WindowWidth / 3);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(TextPosition, WindowPosition.y + 10), ImColor(1.0, 1.0f, 1.0f, 1.0f), combinedText);
        ImGui::GetForegroundDrawList()->AddLine(ImVec2(TextPosition - 5, WindowPosition.y + 23), ImVec2(TextPosition + 346, WindowPosition.y + 23), ImColor(255, 255, 255), 1);

        // Render Right (Main) Panel
        ImGui::SetCursorPos(ImVec2(175, 32));
        colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        if (ImGui::BeginChild("##RightPanel", ImVec2(GUI->WindowWidth - 186, GUI->WindowHeight - 43), true, ImGuiWindowFlags_NoScrollbar)) {
            colors[ImGuiCol_Border] = GUI->DetailColor;
            // Render Tabs
            switch (GUI->CurrentTab) {
                case GUI->MenuTabs::Legitbot:
                    GUI->RenderLegitbot(OverlayWindow);
                    break;
                case GUI->MenuTabs::Ragebot:
                    GUI->RenderRagebot();
                    break;
                case GUI->MenuTabs::Flickbot:
                    GUI->RenderFlickbot();
                    break;
                case GUI->MenuTabs::Triggerbot:
                    GUI->RenderTriggerbot();
                    break;
                case GUI->MenuTabs::Glow:
                    GUI->RenderGlow();
                    break;
                case GUI->MenuTabs::ESP:
                    GUI->RenderESP(OverlayWindow);
                    break;
                case GUI->MenuTabs::Misc:
                    GUI->RenderMisc();
                    break;
                case GUI->MenuTabs::Settings:
                    GUI->RenderSettings();
                    break;
                case GUI->MenuTabs::Config:
                    ImVec2 TabSize;
                    TabSize = ImGui::GetWindowSize();
                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::BeginChild("workzone", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
                    ImGui::SetCursorPos({ 15, 15 });
                    ImGui::BeginChild("workzone", ImVec2(GUI->WindowWidth - 186, GUI->WindowHeight - 60), false, ImGuiWindowFlags_NoScrollbar);

                    ImGui::BeginChildFrame(1, ImVec2(GUI->WindowWidth - 220, GUI->WindowHeight - 73), true);
                    {
                        GUI->DoubleSpacing();
                        Configs->LoadConfigs();
                        Configs->RenderConfigs();

                        ImGui::EndChildFrame();
                    }

                    ImGui::EndChild();
                    ImGui::EndChild();
                    break;
            }
            ImGui::EndChild();
            Legit->UpdateAimList();
            Legit->UpdateRCSList();
            Rage->UpdateRageList();
            Flick->UpdateFlickList();
            Trigger->UpdateWeaponList();
            MiscTab->UpdateRapidFireList();
            GlowESP->ItemGlowSettings();
        }

        // Add Vertical Separator Line
        ImVec2 C = ImGui::GetWindowPos();
        const ImVec2 Point1 = ImVec2(MenuSize.x / 6 + 6.5f + C.x, 32 + C.y);
        const ImVec2 Point2 = ImVec2(MenuSize.x / 6 + 6.5f + C.x, 32 + (MenuSize.y - 43) + C.y);
        ImGui::GetWindowDrawList()->AddLine(Point1, Point2, ImGui::ColorConvertFloat4ToU32(GUI->DetailColor), 1.0f);

        // End Drawlist Calls
        ImGui::End();
    }
}

// Core
bool UpdateCore() {
    try {
        // Map Checking //
        Map->Read();
        if (!Map->IsPlayable) {
            return true;
        }

        // Read Local Player //
        Myself->Read();
        if (!Myself->IsValid()) {
            return true;
        }

        const auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        if (Now >= LastReset + std::chrono::milliseconds(5000)) { // update entire entity list (10k in practice range) every 5s
            Players->clear();
            LastReset = LastRead = Now;
            for (auto p: Map->IsFiringRange ? *Dummies : *HumanPlayers) {
                p->FullRead();
                if (p->BasePointer != 0 && (p->IsPlayer() || p->IsDummy()))
                    Players->push_back(p);
            }
        } else if (Now >= LastRead + std::chrono::milliseconds(50)) { // update known players every tick
            LastRead = Now;
            for (const auto p : *Players)
                p->FullRead();
        } else { // update *some* things constantly to keep ESP & aimbot smooth
            for (const auto p : *Players) { // update important data about known players constantly
                p->ShortRead();
            }
        }

        // Updates //
        GameCamera->Update();
        GlowESP->Update();
        GlowESP->ViewModelGlow();
        Legit->UpdateAimbot();
        Legit->UpdateRCS();
        Rage->Update();
        Trigger->Update();
        Flick->Update();
        MiscTab->Update();
        MapRadar->ActivateBigMap();

        return true;
    }
    catch (const std::exception& ex) {
        std::system("clear");
        std::cout << "Error: " << ex.what() << std::endl;
        return true;
    }

    return false;
}

void LocalPlayerThreadRun() {
    while (!StopThread) {
        if (!Map->IsPlayable) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (!Myself->IsValid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        //----------------------------- SuperGlide -----------------------------
        if (Features::Misc::SuperGlide)
            MiscTab->SuperGlide();
        //----------------------------- BHop -----------------------------
        if (Features::Misc::BHop)
            MiscTab->BHop();
         //----------------------------- Auto Wall Jump -----------------------------
        if (Features::Misc::WallJump)
            MiscTab->WallJump();
         //----------------------------- Auto Tap Strafe -----------------------------
        if (Features::Misc::AutoTapStrafe)
            MiscTab->AutoTapStrafe();
        //----------------------------- QuickTurn -----------------------------
        if (Features::Misc::QuickTurn)
            MiscTab->QuickTurn();
        //----------------------------- RapidFire -----------------------------
        if (Features::Misc::RapidFire)
            MiscTab->RapidFire();
        //----------------------------- Other -----------------------------
    }
}

std::string slurpFile(const std::string& absolutePath) {
    std::string contents;
    std::ifstream file;
    file.open(absolutePath, std::ios::in);

    if (file.fail()) {
        return contents;
    }

    char c;
    while (file.get(c)) {
        contents += c;
    }

    file.close();
    std::erase(contents, '\n');
    std::erase(contents, '\r');
    return contents;
}

// Credits - hir0xy
bool IsOutdated() { // Scan possible Steam installation paths for libraryfolders.vdf to then scan existing library folders for the games "gameversion.txt"
  // Get currently logged in user, since getuid won't work when we're run as root
    struct passwd* pw;
    const char* username = nullptr;
    while ((pw = getpwent()) != nullptr) {
        if (strncmp(pw->pw_dir, "/home/", 6) == 0) {
            username = pw->pw_name;
            break;
        }
    }
    endpwent();

    if (username == nullptr)
        return true;

    const std::string steamPaths[] = {
      "/.steam/steam/config/libraryfolders.vdf",
      "/.local/share/Steam/config/libraryfolders.vdf",
      "/.var/app/com.valvesoftware.Steam/data/Steam/config/libraryfolders.vdf"
    };

    std::vector<std::string> extractedPaths;
    for (const auto& steamPath : steamPaths) {
        std::stringstream fullPath;
        fullPath << "/home/" << username << steamPath;

        std::string libraryfolders = slurpFile(fullPath.str());
        size_t currentPos = 0;
        while (true) {
            const size_t pathPos = libraryfolders.find("path", currentPos);

            if (pathPos == std::string::npos)
                break;

            const size_t pathStart = pathPos + 8;
            const size_t pathEnd = libraryfolders.find('"', pathStart);

            if (pathEnd != std::string::npos) {
                std::string extractedPath = libraryfolders.substr(pathStart, pathEnd - pathStart);
                std::stringstream finalPath;
                finalPath << extractedPath << R"(/steamapps/common/Apex Legends/gameversion.txt)";

                if (std::string version = slurpFile(finalPath.str()); version == GameVersion) {
                    return false;
                }
            }

            currentPos = pathEnd;
        }
    }

    return true;
}

// Main
int main(int argc, char* argv[]) {
    if (getuid()) {
        std::cout << "Please run as sudo!" << std::endl;
        return -1;
    }

    // Waiting for Apex Legends to be found //
    while (Memory::GetPID() == 0) {
        std::system("clear");
        std::cout << "waiting for apex..." << std::endl;
        Sleep(1);
    }

    std::system("clear");
    std::cout << " " << std::endl; // Spacing

    Sleep(1);

    std::cout << "                                                   ████   ███  "
        "                     █████   "
        << std::endl;
    Sleep(50);
    std::cout << "                                                  ░░███  ░░░   "
        "                    ░░███    "
        << std::endl;
    Sleep(50);
    std::cout << " █████████  ██████   ████████              ██████  ░███  ████  "
        " ██████  ████████   ███████  "
        << std::endl;
    Sleep(50);
    std::cout << "░█░░░░███  ░░░░░███ ░░███░░███ ██████████ ███░░███ ░███ ░░███  "
        "███░░███░░███░░███ ░░░███░   "
        << std::endl;
    Sleep(50);
    std::cout << "░   ███░    ███████  ░███ ░███░░░░░░░░░░ ░███ ░░░  ░███  ░███ "
        "░███████  ░███ ░███   ░███    "
        << std::endl;
    Sleep(50);
    std::cout << "  ███░   █ ███░░███  ░███ ░███           ░███  ███ ░███  ░███ "
        "░███░░░   ░███ ░███   ░███ ███"
        << std::endl;
    Sleep(50);
    std::cout << " █████████░░████████ ░███████            ░░██████  █████ "
        "█████░░██████  ████ █████  ░░█████ "
        << std::endl;
    Sleep(50);
    std::cout << "░░░░░░░░░  ░░░░░░░░  ░███░░░              ░░░░░░  ░░░░░ ░░░░░  "
        "░░░░░░  ░░░░ ░░░░░    ░░░░░  "
        << std::endl;
    Sleep(50);
    std::cout << "                     ░███                                      "
        "                             "
        << std::endl;
    Sleep(50);
    std::cout << "                     █████                                     "
        "                             "
        << std::endl;
    Sleep(50);
    std::cout << "                    ░░░░░                                      "
        "                             "
        << std::endl;

    Sleep(400);
    std::cout << "-----------------------------" << std::endl;
    Sleep(50);
    std::cout << "zap client - " << CheatVersion << std::endl;
    Sleep(50);
    std::cout << "By Gerosity" << std::endl;
    Sleep(50);
    std::cout << "For Game Version " << GameVersion << std::endl;
    Sleep(50);
    std::cout << "-----------------------------" << std::endl;
    Sleep(400);

    // Version Check
    std::cout << "Cross-checking Game Verison & Offsets..." << std::endl;
    Sleep(333);
    if (IsOutdated()) {
        std::cout << "Version Check Failed! Offsets Outdated?" << std::endl;
        Sleep(500);
        std::cout << "Continuing..." << std::endl;
    }
    else {
        std::cout << "Cross-check Successful!" << std::endl;
    }

    std::cout << "-----------------------------" << std::endl;
    Sleep(750);

    // Initialize Overlay Window //
    if (!InitializeOverlayWindow())
        return -1;

      // Theading //
    std::thread MenuStateThread(MenuStateRun);
    MenuStateThread.detach();
    std::thread InputManagerThread(InputManager::run);
    InputManagerThread.detach();
    std::thread LocalPlayerThread(LocalPlayerThreadRun);
    LocalPlayerThread.detach();

    // Initialize the whole process //
    try {
        for (int i = 0; i < 70; i++)
            HumanPlayers->push_back(new Player(i, Myself));

        for (int i = 0; i < 10000; i++) // 15000
            Dummies->push_back(new Player(i, Myself));

        std::cout << "Core Initialized!" << std::endl;
        std::cout << "----------- LOGS ------------" << std::endl;

        LoadDefaultConfig();
        // This is where the fun starts //
        OverlayWindow.Start(&UpdateCore, &RenderUI);
        srand(static_cast<unsigned>(time(0)));
    }
    catch (...) {
    }

    StopThread = true;
    InputManager::StopThread = true;
    MenuStateThread.join();
    InputManagerThread.join();
    LocalPlayerThread.join();
    return 0;
};

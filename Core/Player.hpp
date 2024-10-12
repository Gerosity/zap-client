#pragma once
#include <string>
#include "Offsets.hpp"
#include "LocalPlayer.hpp"
#include "Level.hpp"
#include "../Utils/Config.hpp"
#include "../Utils/Features.hpp"
#include "../Utils/Memory.hpp"
#include "../Utils/HitboxType.hpp"
#include "../Utils/Weapons.hpp"
#include "../Math/Vector2D.hpp"
#include "../Math/Vector3D.hpp"
#include "../Math/FloatVector2D.hpp"
#include "../Math/FloatVector3D.hpp"
#include "../Math/Matrix.hpp"
#include "../Overlay/Overlay.hpp"
#include "GlowMode.hpp"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"

struct Player {
    LocalPlayer* Myself;

    int Index;
    long BasePointer;

    std::string Name;
    int Team;

    bool IsDead;
    bool IsKnocked;

    Vector2D ViewAngles;
    float ViewYaw;
    Vector3D LocalOrigin;
    Vector3D AbsoluteVelocity;

    int Health;
    int MaxHealth;
    int Shield;
    int MaxShield;

    int LastTimeAimedAt;
    int LastTimeAimedAtPrevious;
    bool IsAimedAt;

    int LastVisibleTime;
    int LastTimeVisiblePrevious;
    bool IsVisible;

    bool IsLocal;
    bool IsAlly;
    bool IsHostile;
    bool nonBR;
    bool friendly;

    int WeaponIndex;
    bool IsHoldingGrenade;
    long WeaponEntity;

    float DistanceToLocalPlayer;
    float Distance2DToLocalPlayer;

    bool IsLockedOn;

    // For AimbotMode Grinder
    int ducking;
    // bool aimbotLocked;
    FloatVector3D localOrigin_prev;
    FloatVector3D localOrigin;
    FloatVector3D absoluteVelocity;
    FloatVector3D localOrigin_predicted;
    FloatVector2D aimbotDesiredAngles;
    FloatVector2D aimbotDesiredAnglesIncrement;
    FloatVector2D aimbotDesiredAnglesSmoothed;
    float aimbotScore;
    FloatVector2D aimbotDesiredAnglesSmoothedNoRecoil;

    std::chrono::milliseconds LastRead;

    Player(int PlayerIndex, LocalPlayer* Me) {
        this->Index = PlayerIndex;
        this->Myself = Me;
    }

    void Reset() {
        // Reset All Variables
        Index = 0;
        BasePointer = 0;

        std::string Name = "";
        Team = 0;

        IsDead = 0;
        IsKnocked = 0;

        ViewAngles = {};
        ViewYaw = 0;
        LocalOrigin = {};
        AbsoluteVelocity = {};

        Health = 0;
        MaxHealth = 0;
        Shield = 0;
        MaxShield = 0;

        LastTimeAimedAt = 0;
        LastTimeAimedAtPrevious = 0;
        IsAimedAt = 0;

        LastVisibleTime = 0;
        LastTimeVisiblePrevious = 0;
        IsVisible = 0;

        IsLocal = 0;
        IsAlly = 0;
        IsHostile = 0;
        nonBR = 0;
        friendly = 0;

        WeaponIndex = 0;
        IsHoldingGrenade = 0;
        WeaponEntity = 0;

        DistanceToLocalPlayer = 0;
        Distance2DToLocalPlayer = 0;

        IsLockedOn = 0;

        ducking = 0;
        localOrigin_prev = {};
        localOrigin = {};
        absoluteVelocity = {};
        localOrigin_predicted = {};
        aimbotDesiredAngles = {};
        aimbotDesiredAnglesIncrement = {};
        aimbotDesiredAnglesSmoothed = {};
        aimbotScore = 0;
        aimbotDesiredAnglesSmoothedNoRecoil = {};
    }

    void Read() {
        BasePointer = Memory::Read<long>(OFF_REGION + OFF_ENTITY_LIST + ((Index + 1) << 5));
        if (BasePointer == 0)
            return;

        Name = Memory::ReadString(BasePointer + OFF_NAME);
        Team = Memory::Read<int>(BasePointer + OFF_TEAM_NUMBER);

        if (!IsPlayer() && !IsDummy()) { BasePointer = 0; return; }

        IsDead = (IsDummy()) ? false : Memory::Read<short>(BasePointer + OFF_LIFE_STATE) > 0;
        IsKnocked = (IsDummy()) ? false : Memory::Read<short>(BasePointer + OFF_BLEEDOUT_STATE) > 0;

        LocalOrigin = Memory::Read<Vector3D>(BasePointer + OFF_LOCAL_ORIGIN);

        LastTimeAimedAt = Memory::Read<int>(BasePointer + OFF_LAST_AIMEDAT_TIME);
        IsAimedAt = LastTimeAimedAtPrevious < LastTimeAimedAt;
        LastTimeAimedAtPrevious = LastTimeAimedAt;
        float WorldTime = Memory::Read<float>(Myself->BasePointer + OFF_TIME_BASE);
        float Time1 = Memory::Read<float>(BasePointer + OFF_LAST_VISIBLE_TIME);
        IsVisible = (Time1 + 0.2) >= WorldTime || IsAimedAt;

        Health = Memory::Read<int>(BasePointer + OFF_HEALTH);
        MaxHealth = Memory::Read<int>(BasePointer + OFF_MAXHEALTH);
        Shield = Memory::Read<int>(BasePointer + OFF_SHIELD);
        MaxShield = Memory::Read<int>(BasePointer + OFF_MAXSHIELD);

        if (!IsDead && !IsKnocked && IsHostile) {
            long WeaponHandle = Memory::Read<long>(BasePointer + OFF_WEAPON_HANDLE);
            long WeaponHandleMasked = WeaponHandle & 0xffff;
            WeaponEntity = Memory::Read<long>(OFF_REGION + OFF_ENTITY_LIST + (WeaponHandleMasked << 5));

            int OffHandWeaponID = Memory::Read<int>(BasePointer + OFF_OFFHAND_WEAPON);
            IsHoldingGrenade = OffHandWeaponID == -251 ? true : false;

            WeaponIndex = Memory::Read<int>(WeaponEntity + OFF_WEAPON_INDEX);
        }

        if (Myself->IsValid()) {
            IsLocal = Myself->BasePointer == BasePointer;
            IsAlly = IsTeammate();
            IsHostile = !IsAlly;
            DistanceToLocalPlayer = Myself->LocalOrigin.Distance(LocalOrigin);
            Distance2DToLocalPlayer = Myself->LocalOrigin.To2D().Distance(LocalOrigin.To2D());
        }
        
        AbsoluteVelocity = Memory::Read<Vector3D>(BasePointer + OFF_ABSVELOCITY);
        
        // Update Once Per Tick - hir0xygen
        // Reading Most Of The Info Above Every Tick Caused Errors Such As Wrong Weapon IDs And ESP To Be Drawn Wrong
        // If You Only Use Glow (And Maybe Aimbot, Haven't Tested), You May Be Able To Move Info Above To Below To Increase Performance
        if (const auto Now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()); Now >= LastRead + std::chrono::milliseconds(50)) {
            LastRead = Now;
            
            ViewAngles = Memory::Read<Vector2D>(BasePointer + OFF_VIEW_ANGLES);
            ViewYaw = Memory::Read<float>(BasePointer + OFF_YAW);
            if (Myself->IsValid()) {
                if (IsVisible) { // For AimbotMode Grinder
                    aimbotDesiredAngles = calcDesiredAngles();
                    aimbotDesiredAnglesIncrement = calcDesiredAnglesIncrement();
                    aimbotScore = calcAimbotScore();
                }
            }

            // For AimbotMode Grinder
            localOrigin = Memory::Read<FloatVector3D>(BasePointer + OFF_LOCAL_ORIGIN);
            absoluteVelocity = Memory::Read<FloatVector3D>(BasePointer + OFF_ABSVELOCITY);
            FloatVector3D localOrigin_diff = localOrigin.subtract(localOrigin_prev).normalize().multiply(20);
            localOrigin_predicted = localOrigin.add(localOrigin_diff);
            localOrigin_prev = FloatVector3D(localOrigin.x, localOrigin.y, localOrigin.z);
        }
    }

    bool IsSpectating() {
        if (!IsDead)
            return false;
        uint64_t SpectatorList = Memory::Read<uint64_t>(OFF_REGION + OFF_OBSERVER_LIST);
        int PlayerData = Memory::Read<int>(BasePointer + 0x38);
        int SpecIndex = Memory::Read<int>(SpectatorList + PlayerData * 8 + OFF_OBSERVER_LIST_IN_ARRAY);
        uint64_t SpectatorAddr = Memory::Read<uint64_t>(OFF_REGION + OFF_ENTITY_LIST + ((SpecIndex & 0xFFFF) << 5));
        if (SpectatorAddr == Myself->BasePointer)
            return true;
        return false;
    }

    float GetViewYaw() {
        if (!IsDummy() || IsPlayer()) {
            return Memory::Read<float>(BasePointer + OFF_YAW);
        }
        return 0.0f;
    }

    bool IsValid() {
        return BasePointer != 0 && Health > 0 && !IsDead && (IsPlayer() || IsDummy());
    }

    bool IsCombatReady() {
        if (!IsValid())
            return false;
        if (IsDummy())
            return true;
        if (IsDead)
            return false;
        if (IsKnocked)
            return false;
        return true;
    }

    bool IsPlayer() {
        return Name == "player";
    }

    bool IsDummy() {
        return Team == 97;
    }

    bool IsTeammate() {
        if (LvMap::m_mixtape && Myself->Squad == -1) {
            return (Team & 1) == (Myself->Team & 1);
        }
        else {
            return Team == Myself->Team;
        }
    }

    // Bones //
    int GetBoneFromHitbox(HitboxType HitBox) const {
        long ModelPointer = Memory::Read<long>(BasePointer + OFF_STUDIOHDR);
        if (!Memory::IsValidPointer(ModelPointer))
            return -1;

        long StudioHDR = Memory::Read<long>(ModelPointer + 0x8);
        if (!Memory::IsValidPointer(StudioHDR + 0x34))
            return -1;

        auto HitboxCache = Memory::Read<uint16_t>(StudioHDR + 0x34);
        auto HitboxArray = StudioHDR + ((uint16_t) (HitboxCache & 0xFFFE) << (4 * (HitboxCache & 1)));
        if (!Memory::IsValidPointer(HitboxArray + 0x4))
            return -1;

        auto IndexCache = Memory::Read<uint16_t>(HitboxArray + 0x4);
        auto HitboxIndex = ((uint16_t) (IndexCache & 0xFFFE) << (4 * (IndexCache & 1)));
        auto BonePointer = HitboxIndex + HitboxArray + (static_cast<int>(HitBox) * 0x20);
        if (!Memory::IsValidPointer(BonePointer))
            return -1;

        return Memory::Read<uint16_t>(BonePointer);
    }

    Vector3D GetBonePosition(HitboxType HitBox) const {
        Vector3D Offset = Vector3D(0.0f, 0.0f, 0.0f);

        int Bone = GetBoneFromHitbox(HitBox);
        if (Bone < 0 || Bone > 255)
            return LocalOrigin.Add(Offset);

        long BonePtr = Memory::Read<long>(BasePointer + OFF_BONES);
        BonePtr += (Bone * sizeof(Matrix3x4));
        if (!Memory::IsValidPointer(BonePtr))
            return LocalOrigin.Add(Offset);

        Matrix3x4 BoneMatrix = Memory::Read<Matrix3x4>(BonePtr);
        Vector3D BonePosition = BoneMatrix.GetPosition();

        if (!BonePosition.IsValid())
            return LocalOrigin.Add(Vector3D(0, 0, 0));

        BonePosition += LocalOrigin;
        return BonePosition;
    }

    // For AimbotMode Grinder
    float calcDesiredPitch() {
        if (IsLocal)
            return 0;
        const FloatVector3D shift = FloatVector3D(100000, 100000, 100000);
        const FloatVector3D originA = Myself->localOrigin.add(shift);
        const float extraZ = (ducking != -1) ? 10 : 0;
        const FloatVector3D originB = localOrigin_predicted.add(shift).subtract(FloatVector3D(0, 0, extraZ));
        const float deltaZ = originB.z - originA.z;
        const float pitchInRadians = std::atan2(-deltaZ, Distance2DToLocalPlayer);
        const float degrees = pitchInRadians * (180.0f / M_PI);
        return degrees;
    }

    float calcDesiredYaw() {
        if (IsLocal)
            return 0;
        const FloatVector2D shift = FloatVector2D(100000, 100000);
        const FloatVector2D originA = Myself->localOrigin.to2D().add(shift);
        const FloatVector2D originB = localOrigin_predicted.to2D().add(shift);
        const FloatVector2D diff = originB.subtract(originA);
        const double yawInRadians = std::atan2(diff.y, diff.x);
        const float degrees = yawInRadians * (180.0f / M_PI);
        return degrees;
    }

    FloatVector2D calcDesiredAngles() {
        return FloatVector2D(calcDesiredPitch(), calcDesiredYaw());
    }

    FloatVector2D calcDesiredAnglesIncrement() {
        return FloatVector2D(calcPitchIncrement(), calcYawIncrement());
    }

    float calcPitchIncrement() {
        float wayA = aimbotDesiredAngles.x - Myself->viewAngles.x;
        float wayB = 180 - abs(wayA);
        if (wayA > 0 && wayB > 0)
            wayB *= -1;
        if (fabs(wayA) < fabs(wayB))
            return wayA;
        return wayB;
    }

    float calcYawIncrement() {
        float wayA = aimbotDesiredAngles.y - Myself->viewAngles.y;
        float wayB = 360 - abs(wayA);
        if (wayA > 0 && wayB > 0)
            wayB *= -1;
        if (fabs(wayA) < fabs(wayB))
            return wayA;
        return wayB;
    }

    float calcAimbotScore() {
        return (1000 - (fabs(aimbotDesiredAnglesIncrement.x) + fabs(aimbotDesiredAnglesIncrement.y)));
    }

    std::string GetPlayerName() {
        if (IsDummy()) {
            return "Dummie";
        }
        else {
            uintptr_t NameIndex = Memory::Read<uintptr_t>(BasePointer + OFF_NAME_INDEX);
            uintptr_t NameOffset = Memory::Read<uintptr_t>(OFF_REGION + OFF_NAME_LIST + ((NameIndex - 1) * 24));
            std::string PlayerName = Memory::ReadStringSize(NameOffset, 64);
            return PlayerName;
        }
    }

    std::string getPlayerModelName() {
        uintptr_t ModelOffset = Memory::Read<uintptr_t>(BasePointer + OFF_MODELNAME);
        std::string ModelName = Memory::ReadStringSize(ModelOffset, 1024);

        static std::unordered_map<std::string, std::string> ModelNameMap = { {"dummie", "Dummie"}, {"ash", "Ash"}, {"ballistic", "Ballistic"}, {"bangalore", "Bangalore"}, {"bloodhound", "Bloodhound"}, {"catalyst", "Catalyst"}, {"caustic", "Caustic"}, {"conduit", "Conduit"}, {"crypto", "Crypto"}, {"fuse", "Fuse"}, {"gibraltar", "Gibraltar"}, {"horizon", "Horizon"}, {"nova", "Horizon"}, {"holo", "Mirage"}, {"mirage", "Mirage"}, {"lifeline", "Lifeline"}, {"loba", "Loba"}, {"madmaggie", "Mad Maggie"}, {"newcastle", "Newcastle"}, {"octane", "Octane"}, {"pathfinder", "Pathfinder"}, {"rampart", "Rampart"}, {"revenant", "Revenant"}, {"seer", "Seer"}, {"stim", "Octane"}, {"valkyrie", "Valkyrie"}, {"vantage", "Vantage"}, {"wattson", "Wattson"}, {"wraith", "Wraith"}, {"alter", "Alter"}, };

        std::string replacedName = ModelName;
        for (auto& entry : ModelNameMap) {
            if (ModelName.find(entry.first) != std::string::npos) {
                replacedName = entry.second;
                break;
            }
        }

        return replacedName;
    }

    std::string GetWeaponHeldName() {
        // Variables
        // Check if Holding Grenade
        /*if (IsHoldingGrenade) {
            std::string WeaponName = "Throwable";
            return WeaponName;
        }
        else {*/
        static std::unordered_map<int, std::string> WeaponMap = { {WeaponIDs::P2020, "P2020"}, {WeaponIDs::RE45, "RE-45"}, {WeaponIDs::ALTERNATOR, "Alternator"}, {WeaponIDs::R301, "R-301"}, {WeaponIDs::R99, "R-99"}, {WeaponIDs::SPITFIRE, "Spitfire"}, {WeaponIDs::G7, "G7 Scout"}, {WeaponIDs::FLATLINE, "Flatline"}, {WeaponIDs::PROWLER, "Prowler"}, {WeaponIDs::HEMLOCK, "Hemlock"}, {WeaponIDs::REPEATER, "30-30 Repeater"}, {WeaponIDs::RAMPAGE, "Rampage"}, {WeaponIDs::CAR, "CAR-SMG"}, {WeaponIDs::HAVOC, "Havoc"}, {WeaponIDs::DEVOTION, "Devotion"}, {WeaponIDs::LSTAR, "L-STAR"}, {WeaponIDs::TRIPLETAKE, "Triple Take"}, {WeaponIDs::VOLT, "Volt"}, {WeaponIDs::NEMESIS, "Nemesis"}, {WeaponIDs::MOZAMBIQUE, "Mozambique"}, {WeaponIDs::PEACEKEEPER, "Peacekeeper"}, {WeaponIDs::MASTIFF, "Mastiff"}, {WeaponIDs::SENTINEL, "Sentinel"}, {WeaponIDs::CHARGE_RIFLE, "Charge Rifle"}, {WeaponIDs::LONGBOW, "Longbow"}, {WeaponIDs::WINGMAN, "Wingman"}, {WeaponIDs::EVA8, "EVA-8 Auto"}, {WeaponIDs::BOCEK, "Bocek"}, {WeaponIDs::KRABER, "Kraber"}, {WeaponIDs::KNIFE, "Throwing Knife"}, {WeaponIDs::HANDS, "Melee"}, };

        if (auto it = WeaponMap.find(WeaponIndex); it != WeaponMap.end()) {
            return it->second;
        }
        else {
            return "Hands";
        }
    }

    ImColor GetWeaponHeldColor() {
        ImColor LightCol, HeavyCol, EnergyCol, ShotgunCol, SniperCol, LegendaryCol, MeleeCol, ThrowableCol;
        if (IsHoldingGrenade) {
            if (IsHostile) {
                ThrowableCol = ImColor(Features::Colors::Enemy::ThrowableWeaponColor[0], Features::Colors::Enemy::ThrowableWeaponColor[1], Features::Colors::Enemy::ThrowableWeaponColor[2], Features::Colors::Enemy::ThrowableWeaponColor[3]);
                return ThrowableCol;
            }
            else if (!IsHostile) {
                ThrowableCol = ImColor(Features::Colors::Teammate::ThrowableWeaponColor[0], Features::Colors::Teammate::ThrowableWeaponColor[1], Features::Colors::Teammate::ThrowableWeaponColor[2], Features::Colors::Teammate::ThrowableWeaponColor[3]);
                return ThrowableCol;
            }
        }
        else {
            if (IsHostile) {
                LightCol = ImColor(Features::Colors::Enemy::LightWeaponColor[0], Features::Colors::Enemy::LightWeaponColor[1], Features::Colors::Enemy::LightWeaponColor[2], Features::Colors::Enemy::LightWeaponColor[3]);
                HeavyCol = ImColor(Features::Colors::Enemy::HeavyWeaponColor[0], Features::Colors::Enemy::HeavyWeaponColor[1], Features::Colors::Enemy::HeavyWeaponColor[2], Features::Colors::Enemy::HeavyWeaponColor[3]);
                EnergyCol = ImColor(Features::Colors::Enemy::EnergyWeaponColor[0], Features::Colors::Enemy::EnergyWeaponColor[1], Features::Colors::Enemy::EnergyWeaponColor[2], Features::Colors::Enemy::EnergyWeaponColor[3]);
                ShotgunCol = ImColor(Features::Colors::Enemy::ShotgunWeaponColor[0], Features::Colors::Enemy::ShotgunWeaponColor[1], Features::Colors::Enemy::ShotgunWeaponColor[2], Features::Colors::Enemy::ShotgunWeaponColor[3]);
                SniperCol = ImColor(Features::Colors::Enemy::SniperWeaponColor[0], Features::Colors::Enemy::SniperWeaponColor[1], Features::Colors::Enemy::SniperWeaponColor[2], Features::Colors::Enemy::SniperWeaponColor[3]);
                LegendaryCol = ImColor(Features::Colors::Enemy::LegendaryWeaponColor[0], Features::Colors::Enemy::LegendaryWeaponColor[1], Features::Colors::Enemy::LegendaryWeaponColor[2], Features::Colors::Enemy::LegendaryWeaponColor[3]);
                MeleeCol = ImColor(Features::Colors::Enemy::MeleeWeaponColor[0], Features::Colors::Enemy::MeleeWeaponColor[1], Features::Colors::Enemy::MeleeWeaponColor[2], Features::Colors::Enemy::MeleeWeaponColor[3]);
                ThrowableCol = ImColor(Features::Colors::Enemy::ThrowableWeaponColor[0], Features::Colors::Enemy::ThrowableWeaponColor[1], Features::Colors::Enemy::ThrowableWeaponColor[2], Features::Colors::Enemy::ThrowableWeaponColor[3]);
            }
            else if (!IsHostile) {
                LightCol = ImColor(Features::Colors::Teammate::LightWeaponColor[0], Features::Colors::Teammate::LightWeaponColor[1], Features::Colors::Teammate::LightWeaponColor[2], Features::Colors::Teammate::LightWeaponColor[3]);
                HeavyCol = ImColor(Features::Colors::Teammate::HeavyWeaponColor[0], Features::Colors::Teammate::HeavyWeaponColor[1], Features::Colors::Teammate::HeavyWeaponColor[2], Features::Colors::Teammate::HeavyWeaponColor[3]);
                EnergyCol = ImColor(Features::Colors::Teammate::EnergyWeaponColor[0], Features::Colors::Teammate::EnergyWeaponColor[1], Features::Colors::Teammate::EnergyWeaponColor[2], Features::Colors::Teammate::EnergyWeaponColor[3]);
                ShotgunCol = ImColor(Features::Colors::Teammate::ShotgunWeaponColor[0], Features::Colors::Teammate::ShotgunWeaponColor[1], Features::Colors::Teammate::ShotgunWeaponColor[2], Features::Colors::Teammate::ShotgunWeaponColor[3]);
                SniperCol = ImColor(Features::Colors::Teammate::SniperWeaponColor[0], Features::Colors::Teammate::SniperWeaponColor[1], Features::Colors::Teammate::SniperWeaponColor[2], Features::Colors::Teammate::SniperWeaponColor[3]);
                LegendaryCol = ImColor(Features::Colors::Teammate::LegendaryWeaponColor[0], Features::Colors::Teammate::LegendaryWeaponColor[1], Features::Colors::Teammate::LegendaryWeaponColor[2], Features::Colors::Teammate::LegendaryWeaponColor[3]);
                MeleeCol = ImColor(Features::Colors::Teammate::MeleeWeaponColor[0], Features::Colors::Teammate::MeleeWeaponColor[1], Features::Colors::Teammate::MeleeWeaponColor[2], Features::Colors::Teammate::MeleeWeaponColor[3]);
                ThrowableCol = ImColor(Features::Colors::Teammate::ThrowableWeaponColor[0], Features::Colors::Teammate::ThrowableWeaponColor[1], Features::Colors::Teammate::ThrowableWeaponColor[2], Features::Colors::Teammate::ThrowableWeaponColor[3]);
            }

            static std::unordered_map<int, ImColor> WeaponMap = { {WeaponIDs::P2020, LightCol}, {WeaponIDs::RE45, LightCol}, {WeaponIDs::ALTERNATOR, LightCol}, {WeaponIDs::R301, LightCol}, {WeaponIDs::R99, LightCol}, {WeaponIDs::SPITFIRE, LightCol}, {WeaponIDs::G7, LightCol}, {WeaponIDs::FLATLINE, HeavyCol}, {WeaponIDs::PROWLER, HeavyCol}, {WeaponIDs::HEMLOCK, HeavyCol}, {WeaponIDs::REPEATER, HeavyCol}, {WeaponIDs::RAMPAGE, HeavyCol}, {WeaponIDs::CAR, HeavyCol}, {WeaponIDs::HAVOC, EnergyCol}, {WeaponIDs::DEVOTION, EnergyCol}, {WeaponIDs::LSTAR, EnergyCol}, {WeaponIDs::TRIPLETAKE, EnergyCol}, {WeaponIDs::VOLT, EnergyCol}, {WeaponIDs::NEMESIS, EnergyCol}, {WeaponIDs::MOZAMBIQUE, ShotgunCol}, {WeaponIDs::PEACEKEEPER, ShotgunCol}, {WeaponIDs::MASTIFF, ShotgunCol}, {WeaponIDs::SENTINEL, SniperCol}, {WeaponIDs::CHARGE_RIFLE, SniperCol}, {WeaponIDs::LONGBOW, SniperCol}, {WeaponIDs::WINGMAN, LegendaryCol}, {WeaponIDs::EVA8, LegendaryCol}, {WeaponIDs::BOCEK, LegendaryCol}, {WeaponIDs::KRABER, LegendaryCol}, {WeaponIDs::KNIFE, LegendaryCol}, {WeaponIDs::HANDS, MeleeCol}, };

            auto it = WeaponMap.find(WeaponIndex);
            if (it != WeaponMap.end()) {
                return it->second;
            }
            else {
                return ImColor(255, 255, 255);
            }
        }
    }

    ImColor GetShieldColor() {
        // Variables
        ImColor ShieldColor;
        if (MaxShield == 50) { // white
            ShieldColor = ImColor(247, 247, 247);
        }
        else if (MaxShield == 75) { // blue
            ShieldColor = ImColor(39, 178, 255);
        }
        else if (MaxShield == 100) { // purple
            ShieldColor = ImColor(206, 59, 255);
        }
        else if (MaxShield == 125) { // red
            ShieldColor = ImColor(219, 2, 2);
        }
        else {
            ShieldColor = ImColor(247, 247, 247);
        }
        return ShieldColor;
    }

    // DEBUG
    // Bones //
    int GetBoneFromHitboxID(int HitBox) const {
        long ModelPointer = Memory::Read<long>(BasePointer + OFF_STUDIOHDR);
        if (!Memory::IsValidPointer(ModelPointer))
            return -1;

        long StudioHDR = Memory::Read<long>(ModelPointer + 0x8);
        if (!Memory::IsValidPointer(StudioHDR + 0x34))
            return -1;

        auto HitboxCache = Memory::Read<uint16_t>(StudioHDR + 0x34);
        auto HitboxArray = StudioHDR + ((uint16_t) (HitboxCache & 0xFFFE) << (4 * (HitboxCache & 1)));
        if (!Memory::IsValidPointer(HitboxArray + 0x4))
            return -1;

        auto IndexCache = Memory::Read<uint16_t>(HitboxArray + 0x4);
        auto HitboxIndex = ((uint16_t) (IndexCache & 0xFFFE) << (4 * (IndexCache & 1)));
        auto BonePointer = HitboxIndex + HitboxArray + (HitBox * 0x20);
        if (!Memory::IsValidPointer(BonePointer))
            return -1;

        return Memory::Read<uint16_t>(BonePointer);
    }

    Vector3D GetBonePositionID(int HitBox) const {
        Vector3D Offset = Vector3D(0.0f, 0.0f, 0.0f);

        int Bone = GetBoneFromHitboxID(HitBox);
        if (Bone < 0 || Bone > 255)
            return LocalOrigin.Add(Offset);

        long BonePtr = Memory::Read<long>(BasePointer + OFF_BONES);
        BonePtr += (Bone * sizeof(Matrix3x4));
        if (!Memory::IsValidPointer(BonePtr))
            return LocalOrigin.Add(Offset);

        Matrix3x4 BoneMatrix = Memory::Read<Matrix3x4>(BonePtr);
        Vector3D BonePosition = BoneMatrix.GetPosition();

        if (!BonePosition.IsValid())
            return LocalOrigin.Add(Vector3D(0, 0, 0));

        BonePosition += LocalOrigin;
        return BonePosition;
    }
};

#pragma once

#include <string>
#include <string_view>

enum class Alignment {
    Unknown,
    Civilian,
    Mafia,
    Maniac
};

enum class RoleKind {
    Unknown,
    Civilian,
    Mafia,
    Bull,
    Commissioner,
    Doctor,
    Maniac,
    Witness,
    Ninja
};

inline constexpr Alignment alignment_for(RoleKind kind) {
    switch (kind) {
        case RoleKind::Mafia:
        case RoleKind::Ninja:
        case RoleKind::Bull:
            return Alignment::Mafia;
        case RoleKind::Maniac:
            return Alignment::Maniac;
        case RoleKind::Civilian:
        case RoleKind::Commissioner:
        case RoleKind::Doctor:
        case RoleKind::Witness:
            return Alignment::Civilian;
        default:
            return Alignment::Unknown;
    }
}

inline constexpr bool is_mafia_aligned(RoleKind kind) {
    return alignment_for(kind) == Alignment::Mafia;
}

inline constexpr bool is_maniac(RoleKind kind) {
    return kind == RoleKind::Maniac;
}

inline constexpr bool is_commissioner(RoleKind kind) {
    return kind == RoleKind::Commissioner;
}

inline constexpr bool is_doctor(RoleKind kind) {
    return kind == RoleKind::Doctor;
}

inline constexpr bool is_bull(RoleKind kind) {
    return kind == RoleKind::Bull;
}

inline constexpr bool is_witness(RoleKind kind) {
    return kind == RoleKind::Witness;
}

inline constexpr bool is_ninja(RoleKind kind) {
    return kind == RoleKind::Ninja;
}

inline std::string role_name(RoleKind kind) {
    switch (kind) {
        case RoleKind::Civilian: return "Мирный";
        case RoleKind::Mafia: return "Мафия";
        case RoleKind::Bull: return "Бык";
        case RoleKind::Commissioner: return "Комиссар";
        case RoleKind::Doctor: return "Доктор";
        case RoleKind::Maniac: return "Маньяк";
        case RoleKind::Witness: return "Свидетель";
        case RoleKind::Ninja: return "Ниндзя";
        default: return "";
    }
}

inline RoleKind role_kind_from_name(std::string_view name) {
    if (name == "Мирный") return RoleKind::Civilian;
    if (name == "Мафия") return RoleKind::Mafia;
    if (name == "Бык") return RoleKind::Bull;
    if (name == "Комиссар") return RoleKind::Commissioner;
    if (name == "Доктор") return RoleKind::Doctor;
    if (name == "Маньяк") return RoleKind::Maniac;
    if (name == "Свидетель") return RoleKind::Witness;
    if (name == "Ниндзя") return RoleKind::Ninja;
    return RoleKind::Unknown;
}

inline bool prevents_maniac_kill(RoleKind kind) {
    return is_bull(kind);
}

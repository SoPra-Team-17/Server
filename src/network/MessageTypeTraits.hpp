/**
 * @file   MessageTypeTraits.hpp
 * @author Dominik Authaler
 * @date   09.05.2020 (creation)
 * @brief  Declaration of helper type traits to filter messages.
 */

#ifndef SERVER017_MESSAGE_TYPE_TRAITS_HPP
#define SERVER017_MESSAGE_TYPE_TRAITS_HPP

#include <network/messages/Hello.hpp>
#include <network/messages/Reconnect.hpp>
#include <network/messages/ItemChoice.hpp>
#include <network/messages/EquipmentChoice.hpp>
#include <network/messages/GameOperation.hpp>
#include <network/messages/GameLeave.hpp>
#include <network/messages/RequestGamePause.hpp>
#include <network/messages/RequestMetaInformation.hpp>
#include <network/messages/RequestReplay.hpp>

template <typename T>
struct receivableFromPlayer {
    static constexpr bool value = false;
};

template <>
struct receivableFromPlayer<spy::network::messages::Hello> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::Reconnect> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::ItemChoice> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::EquipmentChoice> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::GameOperation> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::GameLeave> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::RequestGamePause> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::RequestMetaInformation> {
    static constexpr bool value = true;
};

template <>
struct receivableFromPlayer<spy::network::messages::RequestReplay> {
    static constexpr bool value = true;
};

template <typename T>
struct receivableFromAI {
    static constexpr bool value = false;
};

template <>
struct receivableFromAI<spy::network::messages::Hello> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::Reconnect> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::ItemChoice> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::EquipmentChoice> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::GameOperation> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::RequestMetaInformation> {
    static constexpr bool value = true;
};

template <>
struct receivableFromAI<spy::network::messages::RequestReplay> {
    static constexpr bool value = true;
};

template <typename T>
struct receivableFromSpectator {
    static constexpr bool value = false;
};

template <>
struct receivableFromSpectator<spy::network::messages::Hello> {
    static constexpr bool value = true;
};

template <>
struct receivableFromSpectator<spy::network::messages::Reconnect> {
    static constexpr bool value = true;
};

template <>
struct receivableFromSpectator<spy::network::messages::GameLeave> {
    static constexpr bool value = true;
};

template <>
struct receivableFromSpectator<spy::network::messages::RequestMetaInformation> {
    static constexpr bool value = true;
};

template <>
struct receivableFromSpectator<spy::network::messages::RequestReplay> {
    static constexpr bool value = true;
};

#endif //SERVER017_MESSAGE_TYPE_TRAITS_HPP

#include "PCH.h"

namespace
{
    class MapMenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (event && event->menuName == RE::MapMenu::MENU_NAME) {
                SKSE::log::info("MapMenu {}", event->opening ? "opened" : "closed");
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message || message->type != SKSE::MessagingInterface::kDataLoaded) { return; }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            SKSE::log::error("Could not register MapMenu diagnostics: UI is unavailable.");
            return;
        }

        static MapMenuEvents events;
        static bool registered = false;
        if (!registered) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&events);
            registered = true;
            SKSE::log::info("Registered MapMenu diagnostics.");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        SKSE::log::error("Could not register the SKSE message listener.");
        return false;
    }
    SKSE::log::info("ArbitraryFastTravel loaded; diagnostic scaffold only.");
    return true;
}

#pragma once
#include "application.hpp"

namespace ggsb {
// Explicit capture fixture, never used on normal startup.
inline void Application::seed_scroll_preview() {
    start_demo(); seed_account_preview();
    const Match sample = *selected();
    const auto sample_session = demo_records.sessions.front();
    for (int i = 0; i < 28; ++i) {
        const auto name = "player" + std::to_string(100 + i), id = "." + std::to_string(200 + i);
        model.users.push_back({name, "+", 2500.0 - i, 2400.0 - i});
        auto match = sample; match.id = id; match.player[0] = name;
        for (int sub = 0; sub < match.games(); ++sub) match.game[sub].id = id + "." + std::to_string(sub);
        model.matches[id] = match; model.watches.insert(id);
        model.requests[id] = {id, name, "*", "s8r14", "15:00//00:02", "R"};
        model.chats.ensure(name);
        model.match_chat[sample.id].push_back(name + ": match chat fixture " + std::to_string(i));
        auto session = sample_session; session.id = "preview-" + std::to_string(i); session.active = false;
        demo_records.sessions.push_back(session);
        RecordedResult result; result.key = "demo|preview-" + std::to_string(i); result.archive_id = id;
        result.scope = sample_session.scope; result.player[0] = sample_session.player; result.player[1] = name;
        result.ended = "2026-10-10 12:34:56"; result.type = "s8r14"; result.result0 = i % 3 ? 2 : 0;
        demo_records.results[result.key] = result;
        for (auto& period : demo_records.sessions) period.included.insert(result.key);
        SavedAccount account{"", "Sample " + std::to_string(i + 1), "skatgame.net", name, "synthetic-password", 5000};
        accounts.save(account, account_error);
    }
    page = 2;
}
}

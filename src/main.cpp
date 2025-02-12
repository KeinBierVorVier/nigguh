#include <dpp/dpp.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <filesystem>

constexpr auto BOT_TOKEN = "token";
constexpr dpp::snowflake ADMIN_ROLE_ID = 1334310177565835324;
constexpr auto DATA_FILE = "nwords.txt";

// Store user data by ID (not username)
std::unordered_map<dpp::snowflake, int> users;
const std::vector<std::string> keywords = {"neg", "nig"};

void save_data() {
    std::ofstream file(DATA_FILE);
    if (!file) {
        return;
    }

    for (const auto &[user_id, count]: users) {
        file << user_id << ":" << count << "\n";
    }
}

void load_data() {
    if (!std::filesystem::exists(DATA_FILE)) {
        return;
    }

    std::ifstream file(DATA_FILE);
    std::string line;

    while (getline(file, line)) {
        size_t delim = line.find(':');
        if (delim == std::string::npos) {
            continue;
        }

        try {
            dpp::snowflake user_id = std::stoull(line.substr(0, delim));
            users[user_id] = std::stoi(line.substr(delim + 1));
        } catch (...) {
            std::cerr << "Error parsing user data, skipping line...\n";
        }
    }
}

// Counts multiple keyword occurrences correctly
int count_keywords(const std::string &msg) {
    int count = 0;
    for (const auto &word: keywords) {
        size_t pos = 0;
        while ((pos = msg.find(word, pos)) != std::string::npos) {
            ++count;
            pos += word.length();
        }
    }
    return count;
}

bool has_admin_role(const dpp::guild_member &member) {
    return std::ranges::any_of(member.get_roles(), [](dpp::snowflake role) {
        return role == ADMIN_ROLE_ID;
    });
}

int main() {
    dpp::cluster bot(BOT_TOKEN, dpp::i_default_intents | dpp::i_message_content);
    load_data();

    bot.on_ready([&bot](const dpp::ready_t &) {
        bot.set_presence({dpp::ps_online, dpp::at_watching, "yo mom..."});
        std::cout << "Bot is online\n";
    });

    bot.on_message_create([&bot](const dpp::message_create_t &event) {
        if (event.msg.author.is_bot()) {
            return;
        }

        dpp::snowflake user_id = event.msg.author.id;
        std::string msg = event.msg.content;
        std::ranges::transform(msg, msg.begin(), ::tolower);

        users.try_emplace(user_id, 0);
        int keyword_count = count_keywords(msg);
        if (keyword_count > 0) {
            users[user_id] += keyword_count;
            save_data();
        }

        if (msg == "!nword" || msg.starts_with("!nword ")) {
            std::string target_user = msg.length() > 7 ? msg.substr(7) : ""; // Extract user if provided

            // Trim spaces
            target_user.erase(0, target_user.find_first_not_of(" \t"));
            target_user.erase(target_user.find_last_not_of(" \t") + 1);

            dpp::snowflake target_id = user_id;

            // Extract user ID from mention
            if (!target_user.empty()) {
                if (target_user.starts_with("<@") && target_user.ends_with(">")) {
                    target_user = target_user.substr(2, target_user.size() - 3);
                }
                try {
                    target_id = dpp::snowflake(std::stoull(target_user));
                } catch (...) {
                    bot.message_create(dpp::message(event.msg.channel_id,
                                                    "Invalid user format. Use `!nword @user` or `!nword`"));
                    return;
                }
            }

            bot.message_create(dpp::message(event.msg.channel_id,
                                            "<@" + std::to_string(target_id) + "> N-Word count: " + std::to_string(
                                                users[target_id])));
        }


        bot.guild_get_member(event.msg.guild_id, user_id,
                             [&bot, event, msg, user_id](const dpp::confirmation_callback_t &cb) {
                                 if (cb.is_error()) {
                                     return;
                                 }

                                 bool is_admin = has_admin_role(std::get<dpp::guild_member>(cb.value));

                                 if (msg.starts_with("!add") && is_admin) {
                                     std::istringstream iss(msg);
                                     std::string cmd, target_user;

                                     int amount;
                                     iss >> cmd >> target_user >> amount;

                                     // Extract user ID from mention
                                     if (target_user.starts_with("<@") && target_user.ends_with(">"))
                                         target_user = target_user.substr(2, target_user.size() - 3);

                                     try {
                                         dpp::snowflake target_id = std::stoull(target_user);
                                         users[target_id] += std::max(amount, 0);

                                         save_data();
                                         bot.message_create(dpp::message(
                                             event.msg.channel_id,
                                             "Added " + std::to_string(amount) + " to <@" + std::to_string(target_id) +
                                             ">"));
                                     } catch (...) {
                                         bot.message_create(
                                             dpp::message(event.msg.channel_id, "Usage: !add @user amount"));
                                     }
                                 }
                             });

        if (msg == "?lb") {
            std::vector<std::pair<std::string, int> > sorted_users;
            std::atomic<int> pending_requests = 0;

            for (const auto &id: users | std::views::keys) {
                pending_requests++;
                bot.user_get(
                    id, [&bot, &sorted_users, &pending_requests, id, event](const dpp::confirmation_callback_t &cb) {
                        if (!cb.is_error()) {
                            const auto *user = std::get_if<dpp::user_identified>(&cb.value);
                            if (user && !user->is_bot()) {
                                sorted_users.emplace_back(user->username, users[id]); // Store username instead of ID
                            }
                        }
                        if (--pending_requests == 0) {
                            // Only proceed when all requests finish
                            if (sorted_users.empty()) {
                                bot.message_create(dpp::message(event.msg.channel_id, "Leaderboard is empty."));
                                return;
                            }

                            std::ranges::sort(sorted_users, std::greater{}, &std::pair<std::string, int>::second);

                            std::ostringstream leaderboard;
                            leaderboard << "```\nLeaderboard:\n";
                            for (size_t i = 0; i < sorted_users.size(); ++i) {
                                leaderboard << (i + 1) << ". " << sorted_users[i].first << ": " << sorted_users[i].
                                        second << "\n";
                            }
                            leaderboard << "```";

                            bot.message_create(dpp::message(event.msg.channel_id, leaderboard.str()));
                        }
                    });
            }

            if (pending_requests == 0) {
                // No users in the system
                bot.message_create(dpp::message(event.msg.channel_id, "Leaderboard is empty."));
            }
        }
    });

    bot.start(dpp::st_wait);
}

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
const std::vector<std::string> keywords = { "neg", "nig" };

void save_data() {
    std::ofstream file(DATA_FILE);
    if (!file) {
        return;
    }

    for (const auto& [user_id, count] : users) {
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
int count_keywords(const std::string& msg) {
    int count = 0;
    for (const auto& word : keywords) {
        size_t pos = 0;
        while ((pos = msg.find(word, pos)) != std::string::npos) {
            ++count;
            pos += word.length();
        }
    }
    return count;
}

bool has_admin_role(const dpp::guild_member& member) {
    return std::ranges::any_of(member.get_roles(), [](dpp::snowflake role) {
        return role == ADMIN_ROLE_ID;
    });
}

int main() {
    dpp::cluster bot(BOT_TOKEN, dpp::i_default_intents | dpp::i_message_content);
    load_data();

    bot.on_ready([&bot](const dpp::ready_t&) {
        bot.set_presence({ dpp::ps_online, dpp::at_watching, "yo mom..." });
        std::cout << "Bot is online\n";
    });

    bot.on_message_create([&bot](const dpp::message_create_t& event) {
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

        if (msg.starts_with("!nword")) {
            bot.message_create(dpp::message(event.msg.channel_id, "<@" + std::to_string(user_id) + "> N-Word count: " + std::to_string(users[user_id])));
        }

        bot.guild_get_member(event.msg.guild_id, user_id, [&bot, event, msg, user_id](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) {
                return;
            }

            bool is_admin = has_admin_role(std::get<dpp::guild_member>(cb.value));

            if (msg.starts_with("!add") && is_admin) {
                std::istringstream iss(msg);
                std::string cmd;
                dpp::snowflake target_id;
                int amount;
                iss >> cmd >> target_id >> amount;

                if (amount <= 0) {
                    bot.message_create(dpp::message(event.msg.channel_id, "Usage: !add <user_id> <amount>"));
                    return;
                }

                users.try_emplace(target_id, 0);
                users[target_id] += amount;
                save_data();

                bot.message_create(dpp::message(event.msg.channel_id, "Added " + std::to_string(amount) + " to <@" + std::to_string(target_id) + ">"));
            }
        });

        if (msg == "?lb") {
            std::vector<std::pair<dpp::snowflake, int>> sorted_users;
            for (const auto& id : users | std::views::keys) {
                bot.user_get(id, [&sorted_users, id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        const auto* user = std::get_if<dpp::user_identified>(&cb.value);
                        if (user && !user->is_bot()) {
                            sorted_users.emplace_back(id, users[id]);
                        }
                    }
                });
            }

            std::ranges::sort(sorted_users, std::greater{}, &std::pair<dpp::snowflake, int>::second);

            std::ostringstream leaderboard;
            leaderboard << "```\nLeaderboard:\n";
            for (size_t i = 0; i < sorted_users.size(); ++i) {
                leaderboard << (i + 1) << ". <@" << sorted_users[i].first << ">: " << sorted_users[i].second << "\n";
            }
            leaderboard << "```";

            bot.message_create(dpp::message(event.msg.channel_id, leaderboard.str()));
        }
    });

    bot.start(dpp::st_wait);
}

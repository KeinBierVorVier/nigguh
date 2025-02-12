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

std::unordered_map<std::string, int> users;
const std::vector<std::string> keywords = { "neg", "nig" };

void save_data() {
    std::ofstream file(DATA_FILE);
    if (!file) {
        return;
    }

    for (const auto& [user, count] : users) {
        file << user << ":" << count << "\n";
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

        users[line.substr(0, delim)] = std::stoi(line.substr(delim + 1));
    }
}

int count_keywords(const std::string& msg) {
    return static_cast<int>(std::ranges::count_if(keywords, [&](const std::string& word) {
        return msg.find(word) != std::string::npos;
    }));
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

        std::string user = event.msg.author.username;
        std::string msg = event.msg.content;
        std::ranges::transform(msg, msg.begin(), ::tolower);

        if (!users.contains(user)) {
            users[user] = 0;
        }

        int keyword_count = count_keywords(msg);
        if (keyword_count > 0) {
            users[user] += keyword_count;
            save_data();
            //bot.message_create(dpp::message(event.msg.channel_id, user + " keyword!"));
        }

        if (msg.starts_with("!nword")) {
            std::istringstream iss(msg);
            std::string command, target_user;
            iss >> command >> target_user;

            std::string username = target_user.empty() ? user : target_user;
            std::string mention = target_user.empty() ? "<@" + std::to_string(event.msg.author.id) + ">" : target_user;

            bot.message_create(dpp::message(event.msg.channel_id, "N-Words from " + mention + ": " + std::to_string(users[username])));
        }

        bot.guild_get_member(event.msg.guild_id, event.msg.author.id, [&bot, event, msg](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) {
                return;
            }

            bool is_admin = has_admin_role(std::get<dpp::guild_member>(cb.value));

            if (msg == "!shutdown" && is_admin) {
                bot.message_create(dpp::message(event.msg.channel_id, "Kys..."));
                save_data();
                bot.shutdown();
            }

            if (msg.starts_with("!add") && is_admin) {
                std::istringstream iss(msg);
                std::string cmd, target_user;

                int amount;
                iss >> cmd >> target_user >> amount;

                if (target_user.empty() || amount <= 0) {
                    bot.message_create(dpp::message(event.msg.channel_id, "Usage: !add <user> <amount>"));
                    return;
                }

                if (!users.contains(target_user)) {
                    users[target_user] = 0;
                }

                users[target_user] += amount;
                save_data();

                bot.message_create(dpp::message(event.msg.channel_id, "Added " + std::to_string(amount) + " to " + target_user));
            }
        });


        if (msg == "?lb") {
            std::vector<std::pair<std::string, int>> sorted_users(users.begin(), users.end());
            std::ranges::sort(sorted_users, std::greater{}, &std::pair<std::string, int>::second);

            std::ostringstream leaderboard;
            leaderboard << "```\nLeaderboard:\n";
            for (size_t i = 0; i < sorted_users.size(); ++i) {
                leaderboard << (i + 1) << ". " << sorted_users[i].first << ": " << sorted_users[i].second << "\n";
            }
            leaderboard << "```";

            bot.message_create(dpp::message(event.msg.channel_id, leaderboard.str()));
        }
    });

    bot.start(dpp::st_wait);
}
